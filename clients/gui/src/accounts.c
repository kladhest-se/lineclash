/* One remembered account, memory-only session, and HTTPS authentication. */
#include "client.h"
#include <ctype.h>
#ifndef PLATFORM_WINDOWS
#include <sys/stat.h>
#endif
void account_erase(void *data,size_t size){volatile unsigned char *p=data;while(size--)*p++=0;}
/* Preserve controlled API errors for every HTTP status; never display HTML/JSON
   error pages or accept success tokens carried by a failed HTTP response. */
int account_http_reply(long status,char *reply,int capacity){
    if(!strncmp(reply,"central-error ",14) || !strncmp(reply,"central-expired ",16))return -1;
    if(status==200 && (!strncmp(reply,"central-session ",16) || !strncmp(reply,"central-registered ",19) || !strncmp(reply,"central-joined ",15) || !strcmp(reply,"central-signed-out") || !strncmp(reply,"central-pin ",12) || !strcmp(reply,"central-pending") || !strcmp(reply,"central-slow-down")))return 1;
    const char *message;
    if(status==503 && strstr(reply,"migration required"))message="Website database migration required: run php bin/manage.php migrate";
    else if(status==503)message="Account service unavailable (HTTP 503). Run website migrations and check the Apache error log.";
    else if(status==429)message="Too many account attempts. Try again in a minute.";
    else if(status==404 || status==400 || status==200)message="Account API is missing or outdated. Refresh the website and try again.";
    else if(status==0)message="Secure account connection failed. Check the network, TLS certificates and system date.";
    else{snprintf(reply,capacity,"central-error Account service returned HTTP %ld; check the website and proxy logs.",status);return -1;}
    snprintf(reply,capacity,"central-error %s",message);return -1;
}
static int hex_token(const char *s){return strlen(s)==64 && strspn(s,"0123456789abcdef")==64;}
static int handle_valid(const char *s){size_t n=strlen(s);return n>=3 && n<=14 && s[0]>='a' && s[0]<='z' && strspn(s,"abcdefghijklmnopqrstuvwxyz0123456789_")==n;}
static int email_valid(const char *s){size_t n=strlen(s);if(!n || n>254 || !strchr(s,'@'))return 0;for(const unsigned char *p=(const unsigned char*)s;*p;p++)if(*p<33 || *p>126)return 0;return 1;}
static void pin_clear(ClientCtx *c){account_erase(c->account_pin_secret,sizeof(c->account_pin_secret));memset(c->account_pin,0,sizeof(c->account_pin));c->account_pin_expires=c->account_pin_next=0;c->account_pin_mode=0;}
static void passwords_clear(ClientCtx *c){account_erase(&c->tf_account_password,sizeof(c->tf_account_password));account_erase(&c->tf_account_confirm,sizeof(c->tf_account_confirm));}
static const char *account_path(void){
    static char path[1024];
#ifdef PLATFORM_AMIGA
    snprintf(path,sizeof(path),"PROGDIR:LineClash.accounts");
#elif defined(PLATFORM_WINDOWS)
    const char *base=getenv("APPDATA");snprintf(path,sizeof(path),"%s\\LineClash\\accounts",base?base:".");
#else
    const char *base=getenv("HOME");snprintf(path,sizeof(path),"%s/.lineclash_accounts",base?base:".");
#endif
    return path;
}
static void account_save(ClientCtx *c){
    FILE *f=fopen(account_path(),"w");if(!f){snprintf(c->account_message,sizeof(c->account_message),"Cannot save account names");return;}
#ifndef PLATFORM_WINDOWS
#ifndef PLATFORM_AMIGA
    chmod(account_path(),0600);
#endif
#endif
    if(c->account.email[0])fprintf(f,"%s\t%s\n",c->account.handle,c->account.email);
    else fprintf(f,"%s\n",c->account.handle);
    fclose(f);
}
void client_accounts_load(ClientCtx *c){
    FILE *f=fopen(account_path(),"r");if(!f)return;char line[384];
    /* Read the first valid entry from older files; the next sign-in saves one. */
    while(fgets(line,sizeof(line),f)){
        line[strcspn(line,"\r\n")]=0;char *email=strchr(line,'\t');if(email)*email++=0;
        if(!handle_valid(line))continue;
        snprintf(c->account.handle,15,"%.14s",line);
        if(email && email_valid(email))snprintf(c->account.email,255,"%.254s",email);
        break;
    }
    fclose(f);
}

static void json_string(char *out,size_t size,const char *in){
    size_t n=0;for(const unsigned char *p=(const unsigned char*)in;*p && n+7<size;p++){
        if(*p=='"'||*p=='\\'){out[n++]='\\';out[n++]=*p;}
        else if(*p<32){snprintf(out+n,size-n,"\\u%04x",*p);n+=6;}
        else out[n++]=*p;
    }out[n]=0;
}
int client_account_ready(ClientCtx *c){return c->account.token[0] && c->account.expires>time(NULL);}
static void account_prefill(ClientCtx *c){
    snprintf(c->tf_account_handle.text,sizeof(c->tf_account_handle.text),"%s",c->account.handle);
    c->tf_account_handle.len=(int)strlen(c->tf_account_handle.text);
    const char *login=c->account.email[0] && strlen(c->account.email)<128?c->account.email:c->account.handle;
    snprintf(c->tf_account_email.text,sizeof(c->tf_account_email.text),"%.127s",login);
    c->tf_account_email.len=(int)strlen(c->tf_account_email.text);
}
void client_accounts_open(ClientCtx *c,int join){
    c->account_return=c->state==CS_ACCOUNT?c->account_return:c->state;
    c->account_join_after=join;c->state=CS_ACCOUNT;c->account_message[0]=0;c->account_create_mode=0;
    pin_clear(c);passwords_clear(c);account_prefill(c);c->tf_account_handle.focused=0;c->tf_account_email.focused=1;
}
void client_accounts_action(ClientCtx *c,int action){
    if(action==ACCOUNT_PIN_CANCEL){pin_clear(c);passwords_clear(c);account_prefill(c);c->tf_account_email.focused=1;c->account_message[0]=0;return;}
    if(account_https_busy())return;
    if(action==ACCOUNT_PIN_START){
        if(client_account_ready(c))return;
        pin_clear(c);passwords_clear(c);c->account_create_mode=0;c->account_pin_mode=1;
#ifdef PLATFORM_AMIGA
        const char *request="{\"action\":\"account-client-pin-begin\",\"client\":\"amiga\"}";
#else
        const char *request="{\"action\":\"account-client-pin-begin\",\"client\":\"desktop\"}";
#endif
        if(account_https_start(request)){c->account_job=5;snprintf(c->account_message,sizeof(c->account_message),"Requesting your sign-in PIN...");}
        else{c->account_pin_mode=0;snprintf(c->account_message,sizeof(c->account_message),"Cannot start secure PIN sign-in");}
        return;
    }
    if(action==ACCOUNT_CREATE_MODE || action==ACCOUNT_LOGIN_MODE){
        if(client_account_ready(c))return;
        pin_clear(c);passwords_clear(c);c->account_create_mode=action==ACCOUNT_CREATE_MODE;c->account_message[0]=0;
        c->tf_account_handle.focused=c->account_create_mode;c->tf_account_email.focused=!c->account_create_mode;
        if(c->account_create_mode){c->tf_account_handle.text[0]=c->tf_account_email.text[0]=0;c->tf_account_handle.len=c->tf_account_email.len=0;}
        if(!c->account_create_mode)account_prefill(c);
        return;
    }
    if(action==ACCOUNT_BACK){pin_clear(c);passwords_clear(c);c->account_join_after=0;c->state=c->account_return==CS_AUTH?CS_SERVER_LIST:c->account_return;return;}
    if(action==ACCOUNT_CONTINUE){
        if(!client_account_ready(c))return;
        c->state=CS_SERVER_LIST;int join=c->account_join_after;c->account_join_after=0;
        if(join)client_on_button(c,BTN_CENTRAL_CONNECT);
        return;
    }
    char body[1024];
    if(action==ACCOUNT_LOGOUT){
        if(c->account.token[0]){
            snprintf(body,sizeof(body),"{\"action\":\"account-client-logout\",\"session\":\"%s\"}",c->account.token);
            if(account_https_start(body)){c->account_job=3;}
            account_erase(body,sizeof(body));
        }
        account_erase(c->account.token,sizeof(c->account.token));c->account.expires=0;c->central_profile[0]=0;
        passwords_clear(c);account_prefill(c);c->tf_account_email.focused=1;
        snprintf(c->account_message,sizeof(c->account_message),"Signed out.");return;
    }
    if(client_account_ready(c) || c->account_pin_mode)return;
    if(action!=ACCOUNT_SIGNIN && action!=ACCOUNT_REGISTER)return;
    int registering=action==ACCOUNT_REGISTER;
    char handle[15];snprintf(handle,sizeof(handle),"%.14s",c->tf_account_handle.text);for(char *p=handle;*p;p++)*p=(char)tolower((unsigned char)*p);
    if(registering && (!handle_valid(handle) || c->tf_account_handle.len>14)){snprintf(c->account_message,sizeof(c->account_message),"Username: 3-14 letters, digits or underscores");return;}
    if(!c->tf_account_password.len || c->tf_account_password.len>72){snprintf(c->account_message,sizeof(c->account_message),"Enter your central account password (up to 72 bytes)");return;}
    char email_input[128];snprintf(email_input,sizeof(email_input),"%s",c->tf_account_email.text);
    char *start=email_input;while(isspace((unsigned char)*start))start++;if(start!=email_input)memmove(email_input,start,strlen(start)+1);
    size_t email_length=strlen(email_input);while(email_length && isspace((unsigned char)email_input[email_length-1]))email_input[--email_length]=0;
    for(char *p=email_input;*p;p++)*p=(char)tolower((unsigned char)*p);
    if(registering && !email_valid(email_input)){snprintf(c->account_message,sizeof(c->account_message),"Enter a valid email address");return;}
    if(registering && strcmp(c->tf_account_password.text,c->tf_account_confirm.text)){snprintf(c->account_message,sizeof(c->account_message),"Passwords do not match");return;}
    const char *login=email_input[0]?email_input:handle;
    if(!registering && !email_valid(login) && !handle_valid(login)){snprintf(c->account_message,sizeof(c->account_message),"Enter your email or username");return;}
    char escaped[512],identifier[384];json_string(escaped,sizeof(escaped),c->tf_account_password.text);json_string(identifier,sizeof(identifier),registering?email_input:login);
    if(registering)snprintf(body,sizeof(body),"{\"action\":\"account-client-register\",\"format\":\"session\",\"handle\":\"%s\",\"email\":\"%s\",\"password\":\"%s\"}",handle,identifier,escaped);
    else snprintf(body,sizeof(body),"{\"action\":\"account-client-login\",\"login\":\"%s\",\"password\":\"%s\"}",identifier,escaped);
    int started=account_https_start(body);account_erase(body,sizeof(body));account_erase(escaped,sizeof(escaped));account_erase(identifier,sizeof(identifier));passwords_clear(c);c->tf_account_password.focused=1;
    if(started){c->account_job=registering?4:1;snprintf(c->account_message,sizeof(c->account_message),"%s",registering?"Creating your account securely...":"Signing in securely over HTTPS...");}
    else snprintf(c->account_message,sizeof(c->account_message),"Cannot start secure sign-in");
}
void client_accounts_key(ClientCtx *c,int sc,int is_text,const char *txt){
    if(c->account_pin_mode){if(sc==41)client_accounts_action(c,ACCOUNT_PIN_CANCEL);return;}
    if(account_https_busy())return;
    if(sc==41){client_accounts_action(c,c->account_create_mode?ACCOUNT_LOGIN_MODE:ACCOUNT_BACK);return;}
    if(client_account_ready(c)){if(sc==40)client_accounts_action(c,ACCOUNT_CONTINUE);return;}
    if(sc==40){client_accounts_action(c,c->account_create_mode?ACCOUNT_REGISTER:ACCOUNT_SIGNIN);return;}
    TextField *all[]={&c->tf_account_handle,&c->tf_account_email,&c->tf_account_password,&c->tf_account_confirm};
    int first=c->account_create_mode?0:1,last=c->account_create_mode?3:2,index=first;
    for(int i=first;i<=last;i++)if(all[i]->focused)index=i;
    TextField *f=all[index];
    if(sc==43){f->focused=0;all[index==last?first:index+1]->focused=1;}
    else if(sc==42 && f->len){do{f->len--;}while(f->len>0 && ((unsigned char)f->text[f->len]&0xc0)==0x80);f->text[f->len]=0;}
    else if(is_text && txt){size_t n=strlen(txt);int limit=index==0?14:index==1?127:72;if(f->len+(int)n<=limit){memcpy(f->text+f->len,txt,n+1);f->len+=(int)n;}}

}
void client_account_challenge(ClientCtx *c,const char *challenge){
    if(!c->central_mode || c->state!=CS_AUTH || c->account_job==2 || !hex_token(challenge))return;
    if(!client_account_ready(c)){snprintf(c->connect_error,sizeof(c->connect_error),"Sign in to your account again");client_on_button(c,BTN_BACK_CONNECT);client_accounts_open(c,1);return;}
    char host[768],body[1024];json_string(host,sizeof(host),c->tf_host.text);
    snprintf(body,sizeof(body),"{\"action\":\"account-client-join\",\"session\":\"%s\",\"challenge\":\"%s\",\"host\":\"%.500s\",\"port\":%d}",c->account.token,challenge,host,atoi(c->tf_port.text));
    if(account_https_start(body)){c->account_job=2;c->account_join_socket=c->sock;}else snprintf(c->connect_error,sizeof(c->connect_error),"Secure sign-in is busy; reconnect");account_erase(body,sizeof(body));
}
void client_accounts_tick(ClientCtx *c){
    if(c->account_pin_mode && c->account_pin_secret[0] && c->account_pin_expires<=time(NULL)){
        account_erase(c->account_pin_secret,sizeof(c->account_pin_secret));
        snprintf(c->account_message,sizeof(c->account_message),"PIN expired. Request a new PIN to try again.");
    }
    if(c->state==CS_ACCOUNT && c->account_pin_mode && c->account_pin_secret[0] && !account_https_busy() && !c->account_job && time(NULL)>=c->account_pin_next){
        char body[192];snprintf(body,sizeof(body),"{\"action\":\"account-client-pin-poll\",\"device\":\"%s\"}",c->account_pin_secret);
        if(account_https_start(body)){c->account_job=6;c->account_pin_next=time(NULL)+5;}account_erase(body,sizeof(body));
    }
    char reply[512];int result=account_https_poll(reply,sizeof(reply));if(!result)return;
    int kind=c->account_job;c->account_job=0;
    if(kind==5 || kind==6){
        if(!c->account_pin_mode || c->state!=CS_ACCOUNT){account_erase(reply,sizeof(reply));return;}
        if(kind==5){
            char secret[65]={0},pin[9]={0};long long expires=0;
            if(result>0 && sscanf(reply,"central-pin %64s %8s %lld",secret,pin,&expires)==3 && hex_token(secret) && strlen(pin)==8 && strspn(pin,"0123456789")==8 && expires>time(NULL) && expires<=time(NULL)+600){
                snprintf(c->account_pin_secret,sizeof(c->account_pin_secret),"%s",secret);snprintf(c->account_pin,sizeof(c->account_pin),"%s",pin);
                c->account_pin_expires=(time_t)expires;c->account_pin_next=time(NULL)+5;
                snprintf(c->account_message,sizeof(c->account_message),"Waiting for approval on the website...");
            }else{snprintf(c->account_message,sizeof(c->account_message),"%.255s",!strncmp(reply,"central-error ",14)?reply+14:"PIN sign-in could not start. Refresh the website.");}
            account_erase(secret,sizeof(secret));account_erase(reply,sizeof(reply));return;
        }
        if(result>0 && (!strcmp(reply,"central-pending") || !strcmp(reply,"central-slow-down"))){c->account_pin_next=time(NULL)+(!strcmp(reply,"central-slow-down")?10:5);account_erase(reply,sizeof(reply));return;}
        if(result<0 || strncmp(reply,"central-session ",16)){
            account_erase(c->account_pin_secret,sizeof(c->account_pin_secret));
            snprintf(c->account_message,sizeof(c->account_message),"%.255s",!strncmp(reply,"central-error ",14)?reply+14:"PIN sign-in failed. Request a new PIN.");account_erase(reply,sizeof(reply));return;
        }
    }
    if(kind==1 || kind==4 || kind==6){char token[65]={0},handle[15]={0},email[255]={0};long long expires=0;
        /* Accept older registration replies during a rolling website update,
           discarding the obsolete field without storing or showing it. */
        int fields=kind==4 && !strncmp(reply,"central-registered ",19)?sscanf(reply,"central-registered %64s %14s %lld %*48s %254s",token,handle,&expires,email):sscanf(reply,"central-session %64s %14s %lld %254s",token,handle,&expires,email);
        int valid=fields>=3 && (kind!=4 || (fields==4 && email_valid(email)));
        if(result>0 && valid && hex_token(token) && handle_valid(handle) && expires>time(NULL)){
            account_erase(&c->account,sizeof(c->account));
            snprintf(c->account.handle,15,"%s",handle);if(email_valid(email))snprintf(c->account.email,255,"%s",email);
            snprintf(c->account.token,65,"%s",token);c->account.expires=(time_t)expires;
            snprintf(c->central_profile,sizeof(c->central_profile),"@%s",handle);account_save(c);
            pin_clear(c);c->account_create_mode=0;client_accounts_action(c,ACCOUNT_CONTINUE);
        }else snprintf(c->account_message,sizeof(c->account_message),"%.255s",!strncmp(reply,"central-error ",14)?reply+14:"Account request failed. Check your details and website status.");
        account_erase(token,sizeof(token));
    }else if(kind==2 && c->state==CS_AUTH && c->central_mode && c->sock==c->account_join_socket && (result<0 || strncmp(reply,"central-joined ",15))){
        if(!strncmp(reply,"central-expired ",16)){account_erase(c->account.token,65);c->account.expires=0;}
        client_on_button(c,BTN_BACK_CONNECT);snprintf(c->connect_error,sizeof(c->connect_error),"%.200s",!strncmp(reply,"central-error ",14)?reply+14:!strncmp(reply,"central-expired ",16)?reply+16:"Secure server sign-in failed; try again");c->state=CS_CONNECT;
    }
    account_erase(reply,sizeof(reply));
}
void client_accounts_shutdown(ClientCtx *c){account_https_shutdown();account_erase(&c->account,sizeof(c->account));pin_clear(c);passwords_clear(c);}
