/* Native public discovery announcements. Network work never runs on the game loop. */
#include "server_internal.h"
#include "config.h"
#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PLATFORM_WINDOWS
#include <winhttp.h>
#include <bcrypt.h>
static HANDLE worker;
static volatile LONG complete;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <curl/curl.h>
static pthread_t worker;
static atomic_int complete;
#endif
static int busy,reported;
static time_t next_announce;
static char payload[1024],token[AUTH_NONCE_LEN];
static long response;
#ifndef PLATFORM_WINDOWS
static size_t discard(char *ptr,size_t size,size_t count,void *unused){(void)ptr;(void)unused;return size*count;}
#endif
static void post(void){
    response=0;
#ifdef PLATFORM_WINDOWS
    HINTERNET session=WinHttpOpen(L"LineClash/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    HINTERNET connection=NULL,request=NULL;
    if(session){WinHttpSetTimeouts(session,3000,3000,3000,5000);connection=WinHttpConnect(session,L"lineclash.kladhest.se",443,0);}
    if(connection)request=WinHttpOpenRequest(connection,L"POST",L"/api/",NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(request){DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
        if(WinHttpSendRequest(request,L"Content-Type: application/json\r\n",(DWORD)-1,payload,(DWORD)strlen(payload),(DWORD)strlen(payload),0)&&WinHttpReceiveResponse(request,NULL)){
            DWORD code=0,size=sizeof(code);if(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&code,&size,NULL))response=code;
        }
    }
    if(request)WinHttpCloseHandle(request);
    if(connection)WinHttpCloseHandle(connection);
    if(session)WinHttpCloseHandle(session);
    InterlockedExchange(&complete,1);
#else
    CURL *curl=curl_easy_init();
    if(curl){struct curl_slist *headers=NULL;headers=curl_slist_append(headers,"Content-Type: application/json");
        curl_easy_setopt(curl,CURLOPT_URL,"https://lineclash.kladhest.se/api/");curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,payload);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,3L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,8L);
        curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,discard);
        if(curl_easy_perform(curl)==CURLE_OK)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&response);
        curl_slist_free_all(headers);curl_easy_cleanup(curl);
    }
    atomic_store(&complete,1);
#endif
}
#ifdef PLATFORM_WINDOWS
static DWORD WINAPI work(LPVOID unused){(void)unused;post();return 0;}
#else
static void *work(void *unused){(void)unused;post();return NULL;}
#endif
static void join_worker(void){
#ifdef PLATFORM_WINDOWS
    WaitForSingleObject(worker,INFINITE);CloseHandle(worker);
#else
    pthread_join(worker,NULL);
#endif
    busy=0;
}
void server_directory_start(void){
    if(!g_cfg.public_server || getenv("LINECLASH_DIRECTORY_DISABLE"))return;
    char path[CFG_MAX_PATH+32];snprintf(path,sizeof(path),"%s.directory-key",g_cfg.db_file);
    FILE *file=fopen(path,"r");if(file){if(!fgets(token,sizeof(token),file))token[0]=0;fclose(file);}
    if(strlen(token)!=64 || strspn(token,"0123456789abcdef")!=64){
        unsigned char random[32];
#ifdef PLATFORM_WINDOWS
        if(BCryptGenRandom(NULL,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)return;
#else
        FILE *entropy=fopen("/dev/urandom","rb");if(!entropy)return;
        size_t got=fread(random,1,sizeof(random),entropy);fclose(entropy);if(got!=sizeof(random))return;
#endif
        for(int i=0;i<32;i++)snprintf(token+i*2,3,"%02x",random[i]);
        file=fopen(path,"w");if(!file){log_error("Public listing disabled: cannot save directory key beside database");return;}
#ifndef PLATFORM_WINDOWS
        chmod(path,0600);
#endif
        if(fputs(token,file)<0){fclose(file);return;}fclose(file);
    }
#ifndef PLATFORM_WINDOWS
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return;
#endif
    next_announce=time(NULL);reported=0;
    log_info("Public discovery enabled; announcing to https://lineclash.kladhest.se (public_server = off disables it)");
}
void server_directory_tick(void){
    if(!next_announce)return;
    if(busy){
#ifdef PLATFORM_WINDOWS
        if(!InterlockedCompareExchange(&complete,0,0))return;
#else
        if(!atomic_load(&complete))return;
#endif
        join_worker();
        if(!reported || response!=200)log_info("Public directory announcement: HTTP %ld%s",response,response==200?"":"; retrying in 60 seconds");
        reported=1;
    }
    time_t now=time(NULL);if(now<next_announce)return;next_announce=now+60;
    int players=0;for(int l=0;l<g_server.lobby_count;l++)for(int p=0;p<MAX_PLAYERS;p++)if(g_server.lobbies[l].players[p].connected)players++;
    char name[512];int j=0;for(int i=0;g_cfg.server_name[i] && j<(int)sizeof(name)-3;i++){unsigned char ch=g_cfg.server_name[i];if(ch<32)continue;if(ch=='"'||ch=='\\')name[j++]='\\';name[j++]=ch;}name[j]=0;
    snprintf(payload,sizeof(payload),"{\"action\":\"public-register\",\"token\":\"%s\",\"name\":\"%s\",\"port\":%d,\"players\":%d,\"capacity\":%d}",token,name,g_cfg.port,players,g_server.lobby_count*g_cfg.max_players_per_room);
#ifdef PLATFORM_WINDOWS
    InterlockedExchange(&complete,0);worker=CreateThread(NULL,0,work,NULL,0,NULL);busy=worker!=NULL;
#else
    atomic_store(&complete,0);busy=pthread_create(&worker,NULL,work,NULL)==0;
#endif
}
void server_directory_stop(void){if(busy)join_worker();next_announce=0;}
