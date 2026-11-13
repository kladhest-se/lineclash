/* Exercise profile selection, password disposal, name-only persistence and join cancellation. */
#include "../clients/gui/src/client.h"
#include <assert.h>
static int pending,reply_ready,connects;
static char request[1024],response[512];
int account_https_busy(void){return pending;}
int account_https_start(const char *body){assert(!pending);snprintf(request,sizeof(request),"%s",body);pending=1;return 1;}
int account_https_poll(char *out,int size){if(!reply_ready)return 0;reply_ready=0;pending=0;snprintf(out,size,"%s",response);return 1;}
void account_https_shutdown(void){pending=0;}
void client_on_button(ClientCtx *c,BtnId button){if(button==BTN_CENTRAL_CONNECT){assert(client_account_ready(c));connects++;}else if(button==BTN_BACK_CONNECT){c->state=CS_SERVER_LIST;c->sock=INVALID_SOCK;}}
static void reply(ClientCtx *c,const char *s){snprintf(response,sizeof(response),"%s",s);reply_ready=1;client_accounts_tick(c);}
static void login(ClientCtx *c,const char *handle,char letter){
    snprintf(c->tf_account_email.text,sizeof(c->tf_account_email.text),"%s@example.test",handle);c->tf_account_email.len=(int)strlen(c->tf_account_email.text);
    snprintf(c->tf_account_password.text,sizeof(c->tf_account_password.text),"%s","a\"b\\c-password");c->tf_account_password.len=(int)strlen(c->tf_account_password.text);
    client_accounts_action(c,ACCOUNT_SIGNIN);assert(pending && strstr(request,"a\\\"b\\\\c-password"));
    assert(!c->tf_account_password.text[0]);char token[65],body[256];memset(token,letter,64);token[64]=0;
    snprintf(body,sizeof(body),"central-session %s %s %lld %s@example.test",token,handle,(long long)time(NULL)+600,handle);reply(c,body);
}
static void logout(ClientCtx *c){client_accounts_open(c,0);client_accounts_action(c,ACCOUNT_LOGOUT);assert(!client_account_ready(c) && !c->account.token[0] && !c->central_profile[0]);reply(c,"central-signed-out");}
int main(int argc,char **argv){
    assert(argc==2);ClientCtx c;memset(&c,0,sizeof(c));c.state=CS_SERVER_LIST;c.sock=INVALID_SOCK;
    FILE *f=fopen(argv[1],"w");assert(f);fputs("legacy\tlegacy@example.test\nother\tother@example.test\n",f);fclose(f);
    client_accounts_load(&c);assert(!client_account_ready(&c) && !strcmp(c.account.handle,"legacy"));
    client_accounts_open(&c,1);assert(!strcmp(c.tf_account_email.text,"legacy@example.test"));login(&c,"alpha",'a');
    assert(connects==1 && client_account_ready(&c) && !strcmp(c.central_profile,"@alpha") && c.state==CS_SERVER_LIST);
    client_accounts_open(&c,0);client_accounts_action(&c,ACCOUNT_CREATE_MODE);assert(!c.account_create_mode);
    logout(&c);login(&c,"beta",'b');assert(!strcmp(c.account.handle,"beta") && !strcmp(c.central_profile,"@beta") && c.state==CS_SERVER_LIST);
    c.state=CS_AUTH;c.central_mode=1;c.sock=10;strcpy(c.tf_host.text,"127.0.0.1");strcpy(c.tf_port.text,"31457");
    char challenge[65];memset(challenge,'c',64);challenge[64]=0;client_account_challenge(&c,challenge);
    assert(pending && strstr(request,"account-client-join") && strstr(request,"127.0.0.1") && !strstr(request,"password"));reply(&c,"central-joined beta");assert(c.state==CS_AUTH);
    client_account_challenge(&c,challenge);c.state=CS_SERVER_LIST;c.sock=INVALID_SOCK;reply(&c,"central-error Outdated request");assert(c.state==CS_SERVER_LIST);
    f=fopen(argv[1],"r");assert(f);char saved[512]={0};assert(fread(saved,1,sizeof(saved)-1,f)>0);fclose(f);assert(!strcmp(saved,"beta\tbeta@example.test\n"));
    ClientCtx boot;memset(&boot,0,sizeof(boot));client_accounts_load(&boot);assert(!boot.account.token[0] && !strcmp(boot.account.handle,"beta"));
    logout(&c);client_accounts_action(&c,ACCOUNT_CREATE_MODE);assert(c.account_create_mode);
    strcpy(c.tf_account_handle.text,"gamma");c.tf_account_handle.len=5;strcpy(c.tf_account_email.text,"gamma@example.test");c.tf_account_email.len=18;
    strcpy(c.tf_account_password.text,"new-password-value");c.tf_account_password.len=18;
    strcpy(c.tf_account_confirm.text,"different");c.tf_account_confirm.len=9;
    client_accounts_action(&c,ACCOUNT_REGISTER);assert(!pending && strstr(c.account_message,"do not match"));
    strcpy(c.tf_account_confirm.text,c.tf_account_password.text);c.tf_account_confirm.len=c.tf_account_password.len;c.account_join_after=1;
    client_accounts_action(&c,ACCOUNT_REGISTER);assert(pending && strstr(request,"account-client-register") && strstr(request,"gamma@example.test"));assert(!c.tf_account_password.text[0] && !c.tf_account_confirm.text[0]);
    char registered[512],token[65],recovery[49];memset(token,'d',64);token[64]=0;memset(recovery,'e',48);recovery[48]=0;
    snprintf(registered,sizeof(registered),"central-registered %s gamma %lld %s gamma@example.test",token,(long long)time(NULL)+600,recovery);
    int previous_connects=connects;reply(&c,registered);assert(c.account_recovery[0] && connects==previous_connects && c.state==CS_ACCOUNT);
    f=fopen(argv[1],"r");assert(f);memset(saved,0,sizeof(saved));fread(saved,1,sizeof(saved)-1,f);fclose(f);assert(!strcmp(saved,"gamma\tgamma@example.test\n") && !strstr(saved,token) && !strstr(saved,recovery));
    client_accounts_action(&c,ACCOUNT_BACK);assert(c.account_recovery[0] && c.state==CS_ACCOUNT);
    client_accounts_action(&c,ACCOUNT_RECOVERY_DONE);assert(!c.account_recovery[0] && connects==previous_connects+1 && !strcmp(c.central_profile,"@gamma"));
    client_accounts_shutdown(&c);assert(!c.account.handle[0]);puts("PASS: one remembered account, legacy import, automatic login, logout, secret disposal, registration and stale join protection");return 0;
}
