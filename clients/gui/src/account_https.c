/* Credentials and bearer sessions go only to the fixed central HTTPS endpoint. */
#include "client.h"
#include <SDL.h>
#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif
static struct {SDL_Thread *thread;SDL_atomic_t done;char body[1024],reply[512];size_t used;int ok;} auth;
#ifndef PLATFORM_WINDOWS
static size_t auth_read(char *data,size_t size,size_t count,void *unused){(void)unused;size_t n=size*count;if(n>=sizeof(auth.reply)-auth.used || memchr(data,0,n))return 0;memcpy(auth.reply+auth.used,data,n);auth.used+=n;auth.reply[auth.used]=0;return n;}
#endif
static int auth_work(void *unused){
    (void)unused;long status=0;int completed=0;
#ifdef PLATFORM_WINDOWS
    HINTERNET s=WinHttpOpen(L"LineClash accounts/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0),c=NULL,r=NULL;
    if(s){WinHttpSetTimeouts(s,5000,5000,5000,10000);c=WinHttpConnect(s,L"lineclash.kladhest.se",443,0);}
    if(c)r=WinHttpOpenRequest(c,L"POST",L"/api/",NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(r){DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(r,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
        if(WinHttpSendRequest(r,L"Content-Type: application/json\r\n",(DWORD)-1,auth.body,(DWORD)strlen(auth.body),(DWORD)strlen(auth.body),0) && WinHttpReceiveResponse(r,NULL)){
            DWORD response_status=0,size=sizeof(response_status);WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&response_status,&size,NULL);status=response_status;
            completed=1;
            for(;;){DWORD n=0;char buffer[512];if(!WinHttpReadData(r,buffer,sizeof(buffer),&n)){completed=0;break;}if(!n)break;if(n>=sizeof(auth.reply)-auth.used || memchr(buffer,0,n)){completed=0;break;}memcpy(auth.reply+auth.used,buffer,n);auth.used+=n;auth.reply[auth.used]=0;}
        }
    }
    if(r)WinHttpCloseHandle(r);if(c)WinHttpCloseHandle(c);if(s)WinHttpCloseHandle(s);
#else
    CURL *c=curl_easy_init();if(c){
        struct curl_slist *headers=curl_slist_append(NULL,"Content-Type: application/json");
        curl_easy_setopt(c,CURLOPT_URL,"https://lineclash.kladhest.se/api/");curl_easy_setopt(c,CURLOPT_POSTFIELDS,auth.body);curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
        curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
        curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,5L);curl_easy_setopt(c,CURLOPT_TIMEOUT,15L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,auth_read);
        CURLcode result=curl_easy_perform(c);curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);completed=result==CURLE_OK;
        curl_easy_cleanup(c);curl_slist_free_all(headers);
    }
#endif
    auth.ok=account_http_reply(completed || status>=400?status:0,auth.reply,sizeof(auth.reply))>0;
    account_erase(auth.body,sizeof(auth.body));SDL_AtomicSet(&auth.done,1);return 0;
}
int account_https_busy(void){return auth.thread!=NULL;}
int account_https_start(const char *body){
    if(auth.thread || strlen(body)>=sizeof(auth.body))return 0;
#ifndef PLATFORM_WINDOWS
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return 0;
#endif
    auth.used=0;auth.ok=0;auth.reply[0]=0;snprintf(auth.body,sizeof(auth.body),"%s",body);SDL_AtomicSet(&auth.done,0);auth.thread=SDL_CreateThread(auth_work,"account HTTPS",NULL);
    if(!auth.thread)account_erase(auth.body,sizeof(auth.body));return auth.thread!=NULL;
}
int account_https_poll(char *reply,int size){if(!auth.thread || !SDL_AtomicGet(&auth.done))return 0;SDL_WaitThread(auth.thread,NULL);auth.thread=NULL;snprintf(reply,size,"%s",auth.reply);int ok=auth.ok;account_erase(auth.reply,sizeof(auth.reply));return ok?1:-1;}
void account_https_shutdown(void){if(auth.thread){SDL_WaitThread(auth.thread,NULL);auth.thread=NULL;}account_erase(auth.body,sizeof(auth.body));account_erase(auth.reply,sizeof(auth.reply));}
