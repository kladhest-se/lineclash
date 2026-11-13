/* Saved account names, memory-only sessions, and connection-bound HTTPS sign-in. */
#include "client.h"
#include <ctype.h>
#ifndef PLATFORM_WINDOWS
#include <sys/stat.h>
#endif
void account_erase(void *data,size_t size){volatile unsigned char *p=data;while(size--)*p++=0;}
static int hex_token(const char *s){return strlen(s)==64 && strspn(s,"0123456789abcdef")==64;}
static int handle_valid(const char *s){size_t n=strlen(s);return n>=3 && n<=14 && s[0]>='a' && s[0]<='z' && strspn(s,"abcdefghijklmnopqrstuvwxyz0123456789_")==n;}
static const char *profiles_path(void){
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
static void profiles_save(ClientCtx *c){
    FILE *f=fopen(profiles_path(),"w");if(!f){snprintf(c->account_message,sizeof(c->account_message),"Cannot save account names");return;}
#ifndef PLATFORM_WINDOWS
#ifndef PLATFORM_AMIGA
    chmod(profiles_path(),0600);
#endif
#endif
    for(int i=0;i<c->account_count;i++)fprintf(f,"%s\n",c->accounts[i].handle);
    fclose(f);
}
void client_accounts_load(ClientCtx *c){
    c->account_active=-1;c->account_cursor=-1;
    FILE *f=fopen(profiles_path(),"r");if(!f)return;char line[128];
    while(fgets(line,sizeof(line),f) && c->account_count<8){line[strcspn(line,"\r\n")]=0;if(!handle_valid(line))continue;int duplicate=0;for(int i=0;i<c->account_count;i++)if(!strcmp(c->accounts[i].handle,line))duplicate=1;if(!duplicate)snprintf(c->accounts[c->account_count++].handle,15,"%.14s",line);}
    fclose(f);
}
static void json_string(char *out,size_t size,const char *in){
    size_t n=0;for(const unsigned char *p=(const unsigned char*)in;*p && n+7<size;p++){
        if(*p=='"'||*p=='\\'){out[n++]='\\';out[n++]=*p;}
        else if(*p<32){snprintf(out+n,size-n,"\\u%04x",*p);n+=6;}
        else out[n++]=*p;
    }out[n]=0;
}
int client_account_ready(ClientCtx *c){return c->account_active>=0 && c->account_active<c->account_count && c->accounts[c->account_active].token[0] && c->accounts[c->account_active].expires>time(NULL);}
void client_accounts_open(ClientCtx *c,int join){
    c->account_return=c->state==CS_ACCOUNT?c->account_return:c->state;
    c->account_join_after=join;c->state=CS_ACCOUNT;c->account_message[0]=0;
    c->tf_account_handle.focused=1;c->tf_account_password.focused=0;
    if(c->account_cursor>=0 && c->account_cursor<c->account_count){snprintf(c->tf_account_handle.text,sizeof(c->tf_account_handle.text),"%s",c->accounts[c->account_cursor].handle);c->tf_account_handle.len=(int)strlen(c->tf_account_handle.text);}
}
void client_accounts_select(ClientCtx *c,int index){
    if(account_https_busy() || index<0 || index>=c->account_count)return;
    c->account_cursor=index;snprintf(c->tf_account_handle.text,sizeof(c->tf_account_handle.text),"%s",c->accounts[index].handle);c->tf_account_handle.len=(int)strlen(c->tf_account_handle.text);
    account_erase(&c->tf_account_password,sizeof(c->tf_account_password));c->tf_account_handle.focused=0;c->tf_account_password.focused=1;c->account_message[0]=0;
}
void client_accounts_action(ClientCtx *c,int action){
    if(account_https_busy())return;
    if(action==ACCOUNT_BACK){account_erase(&c->tf_account_password,sizeof(c->tf_account_password));c->account_join_after=0;c->state=c->account_return==CS_AUTH?CS_SERVER_LIST:c->account_return;return;}
    int index=c->account_cursor;
    if(action==ACCOUNT_USE){
        if(index<0 || index>=c->account_count || !c->accounts[index].token[0] || c->accounts[index].expires<=time(NULL)){snprintf(c->account_message,sizeof(c->account_message),"Enter this account's password and sign in first");return;}
        c->account_active=index;snprintf(c->central_profile,sizeof(c->central_profile),"@%s",c->accounts[index].handle);
        c->state=c->account_return;int join=c->account_join_after;c->account_join_after=0;if(join)client_on_button(c,BTN_CENTRAL_CONNECT);return;
    }
    char body[1024];
    if(action==ACCOUNT_REMOVE || action==ACCOUNT_LOGOUT){
        if(index<0 || index>=c->account_count)return;
        if(c->accounts[index].token[0]){snprintf(body,sizeof(body),"{\"action\":\"account-client-logout\",\"session\":\"%s\"}",c->accounts[index].token);if(account_https_start(body))c->account_job=3;account_erase(body,sizeof(body));}
        account_erase(c->accounts[index].token,sizeof(c->accounts[index].token));c->accounts[index].expires=0;
        if(c->account_active==index){c->account_active=-1;c->central_profile[0]=0;}
        if(action==ACCOUNT_REMOVE){memmove(&c->accounts[index],&c->accounts[index+1],(c->account_count-index-1)*sizeof(c->accounts[0]));account_erase(&c->accounts[--c->account_count],sizeof(c->accounts[0]));if(c->account_active>index)c->account_active--;c->account_cursor=-1;c->tf_account_handle.text[0]=0;c->tf_account_handle.len=0;profiles_save(c);}
        snprintf(c->account_message,sizeof(c->account_message),"%s",action==ACCOUNT_REMOVE?"Account removed from this client":"Signed out on this client");return;
    }
    if(action!=ACCOUNT_SIGNIN)return;
    char handle[15];snprintf(handle,sizeof(handle),"%.14s",c->tf_account_handle.text);for(char *p=handle;*p;p++)*p=(char)tolower((unsigned char)*p);
    if(!handle_valid(handle) || c->tf_account_handle.len>14){snprintf(c->account_message,sizeof(c->account_message),"Username: 3-14 letters, digits or underscores");return;}
    if(!c->tf_account_password.len || c->tf_account_password.len>72){snprintf(c->account_message,sizeof(c->account_message),"Enter your central account password (up to 72 bytes)");return;}
    int exists=0;for(int i=0;i<c->account_count;i++)if(!strcmp(handle,c->accounts[i].handle))exists=1;
    if(!exists && c->account_count==8){snprintf(c->account_message,sizeof(c->account_message),"Remove an account before adding another (limit 8)");return;}
    char escaped[512];json_string(escaped,sizeof(escaped),c->tf_account_password.text);
    snprintf(body,sizeof(body),"{\"action\":\"account-client-login\",\"handle\":\"%s\",\"password\":\"%s\"}",handle,escaped);
    int started=account_https_start(body);account_erase(body,sizeof(body));account_erase(escaped,sizeof(escaped));account_erase(&c->tf_account_password,sizeof(c->tf_account_password));c->tf_account_password.focused=1;
    if(started){c->account_job=1;snprintf(c->account_message,sizeof(c->account_message),"Signing in securely over HTTPS...");}
    else snprintf(c->account_message,sizeof(c->account_message),"Cannot start secure sign-in");
}
void client_accounts_key(ClientCtx *c,int sc,int is_text,const char *txt){
    if(account_https_busy())return;
    if(sc==41){client_accounts_action(c,ACCOUNT_BACK);return;}
    if(sc==40){client_accounts_action(c,ACCOUNT_SIGNIN);return;}
    TextField *f=c->tf_account_password.focused?&c->tf_account_password:&c->tf_account_handle;
    if(sc==43){c->tf_account_password.focused=!c->tf_account_password.focused;c->tf_account_handle.focused=!c->tf_account_password.focused;}
    else if(sc==42 && f->len){do{f->len--;}while(f->len>0 && ((unsigned char)f->text[f->len]&0xc0)==0x80);f->text[f->len]=0;}
    else if(is_text && txt){size_t n=strlen(txt);int limit=f==&c->tf_account_handle?14:72;if(f->len+(int)n<=limit){memcpy(f->text+f->len,txt,n+1);f->len+=(int)n;}}
}
void client_account_challenge(ClientCtx *c,const char *challenge){
    if(!c->central_mode || c->state!=CS_AUTH || c->account_job==2 || !hex_token(challenge))return;
    if(!client_account_ready(c)){snprintf(c->connect_error,sizeof(c->connect_error),"Sign in to your account again");client_on_button(c,BTN_BACK_CONNECT);client_accounts_open(c,1);return;}
    char host[768],body[1024];json_string(host,sizeof(host),c->tf_host.text);
    snprintf(body,sizeof(body),"{\"action\":\"account-client-join\",\"session\":\"%s\",\"challenge\":\"%s\",\"host\":\"%.500s\",\"port\":%d}",c->accounts[c->account_active].token,challenge,host,atoi(c->tf_port.text));
    if(account_https_start(body)){c->account_job=2;c->account_join_socket=c->sock;}else snprintf(c->connect_error,sizeof(c->connect_error),"Secure sign-in is busy; reconnect");account_erase(body,sizeof(body));
}
void client_accounts_tick(ClientCtx *c){
    char reply[512];int result=account_https_poll(reply,sizeof(reply));if(!result)return;
    int kind=c->account_job;c->account_job=0;
    if(kind==1){char token[65],handle[15];long long expires=0;
        if(result>0 && sscanf(reply,"central-session %64s %14s %lld",token,handle,&expires)==3 && hex_token(token) && handle_valid(handle) && expires>time(NULL)){
            int i;for(i=0;i<c->account_count;i++)if(!strcmp(c->accounts[i].handle,handle))break;
            if(i<8){if(i==c->account_count)c->account_count++;snprintf(c->accounts[i].handle,15,"%s",handle);snprintf(c->accounts[i].token,65,"%s",token);c->accounts[i].expires=(time_t)expires;c->account_cursor=i;profiles_save(c);snprintf(c->account_message,sizeof(c->account_message),"Signed in. Select Use account to make it active.");if(c->account_join_after)client_accounts_action(c,ACCOUNT_USE);}
        }else snprintf(c->account_message,sizeof(c->account_message),"%.255s",!strncmp(reply,"central-error ",14)?reply+14:"Sign-in failed. Check your password, TLS and website status.");
        account_erase(token,sizeof(token));
    }else if(kind==2 && c->state==CS_AUTH && c->central_mode && c->sock==c->account_join_socket && (result<0 || strncmp(reply,"central-joined ",15))){
        if(!strncmp(reply,"central-expired ",16) && c->account_active>=0){account_erase(c->accounts[c->account_active].token,65);c->accounts[c->account_active].expires=0;}
        client_on_button(c,BTN_BACK_CONNECT);snprintf(c->connect_error,sizeof(c->connect_error),"%.200s",!strncmp(reply,"central-error ",14)?reply+14:!strncmp(reply,"central-expired ",16)?reply+16:"Secure server sign-in failed; try again");c->state=CS_CONNECT;
    }
    account_erase(reply,sizeof(reply));
}
void client_accounts_shutdown(ClientCtx *c){account_https_shutdown();account_erase(c->accounts,sizeof(c->accounts));account_erase(&c->tf_account_password,sizeof(c->tf_account_password));}
