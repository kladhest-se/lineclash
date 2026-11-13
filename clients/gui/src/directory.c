/* Bounded HTTPS discovery on an SDL worker; no credentials are sent. */
#include "client.h"
#include <SDL.h>
#include <ctype.h>
#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif
#define FEED_LIMIT 65536
static struct { SDL_Thread *thread; SDL_atomic_t done; int lan; char url[2048],body[FEED_LIMIT],error[128]; size_t used; } job;
int directory_url_valid(const char *url,int local) {
    if(!url || strlen(url)>1900) return 0;
    for(const unsigned char *p=(const unsigned char*)url;*p;p++) if(*p<33 || *p==127 || *p=='@' || *p=='#' || *p=='?' || *p=='\\') return 0;
    if(!strncmp(url,"https://",8)) return url[8] && url[8]!='/';
    if(local && !strncmp(url,"http://",7)) {
        const char *h=url+7; const char *ends[]={"127.0.0.1","localhost","[::1]"};
        for(int i=0;i<3;i++) { size_t n=strlen(ends[i]); if(!strncmp(h,ends[i],n) && (h[n]==':' || h[n]=='/' || h[n]==0)) return 1; }
    }
    return 0;
}
#ifndef PLATFORM_WINDOWS
static size_t receive_body(char *ptr,size_t size,size_t count,void *unused) {
    (void)unused; size_t n=size*count;
    if(n>=sizeof(job.body)-job.used || memchr(ptr,0,n)) return 0;
    memcpy(job.body+job.used,ptr,n); job.used+=n; job.body[job.used]=0; return n;
}
#endif
static int fetch(void *unused) {
    (void)unused; int ok=0;
#ifdef PLATFORM_WINDOWS
    WCHAR wide[2048],host[512],path[2048]; URL_COMPONENTS parts; memset(&parts,0,sizeof(parts));
    parts.dwStructSize=sizeof(parts); parts.lpszHostName=host;parts.dwHostNameLength=512;parts.lpszUrlPath=path;parts.dwUrlPathLength=2048;
    MultiByteToWideChar(CP_UTF8,0,job.url,-1,wide,2048);
    HINTERNET session=NULL,connection=NULL,request=NULL;
    if(WinHttpCrackUrl(wide,0,0,&parts) && parts.dwHostNameLength<512 && parts.dwUrlPathLength<2000) {
        host[parts.dwHostNameLength]=0;path[parts.dwUrlPathLength]=0;
        /* Include the known fixed query explicitly: WinHTTP URL path excludes it. */
        wcscat(path,L"?action=servers&format=text");
        session=WinHttpOpen(L"LineClash directory/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(session) { WinHttpSetTimeouts(session,3000,3000,3000,3000); connection=WinHttpConnect(session,host,parts.nPort,0); }
        if(connection) request=WinHttpOpenRequest(connection,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0);
        DWORD disable=WINHTTP_DISABLE_REDIRECTS;
        if(request) WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof(disable));
        if(request && WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) && WinHttpReceiveResponse(request,NULL)) {
            DWORD status=0,len=sizeof(status);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&len,WINHTTP_NO_HEADER_INDEX);
            if(status==200) { ok=1; for(;;) { DWORD got=0;char buf[4096];if(!WinHttpReadData(request,buf,sizeof(buf),&got)){ok=0;break;}if(!got)break;if(got>=sizeof(job.body)-job.used || memchr(buf,0,got)){ok=0;break;}memcpy(job.body+job.used,buf,got);job.used+=got; }job.body[job.used]=0; }
        }
    }
    if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);if(session)WinHttpCloseHandle(session);
#else
    CURL *c=curl_easy_init();
    if(c) {
        char url[2048];snprintf(url,sizeof(url),"%s?action=servers&format=text",job.url);
        curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive_body);
        curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT_MS,3000L);curl_easy_setopt(c,CURLOPT_TIMEOUT_MS,6000L);
        curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_USERAGENT,"LineClash directory/1");
        CURLcode result=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);ok=result==CURLE_OK && status==200;curl_easy_cleanup(c);
    }
#endif
    if(!ok) snprintf(job.error,sizeof(job.error),"Discovery failed: check URL, TLS and website status");
    SDL_AtomicSet(&job.done,1);return 0;
}
int directory_parse(char *body,SavedServer *rows,int capacity) {
    if(strncmp(body,"LC-DIRECTORY-1\n",15)) return -1;
    int count=0;char *line=body+15;
    while(*line) {
        char *end=strchr(line,'\n');if(!end)return -1;*end=0;
        if(!*line){line=end+1;continue;}
        char *fields[6];fields[0]=line;
        for(int i=1;i<6;i++){char *tab=strchr(fields[i-1],'\t');if(!tab)return -1;*tab=0;fields[i]=tab+1;}
        if(strchr(fields[5],'\t') || !fields[2][0] || strlen(fields[2])>127) return -1;
        for(const char *p=fields[2];*p;p++) if(!isalnum((unsigned char)*p) && !strchr(".-:[]",*p)) return -1;
        for(int i=0;i<6;i++)for(const unsigned char *p=(unsigned char*)fields[i];*p;p++)if(*p<32 || *p==127)return -1;
        char *tail;long port=strtol(fields[3],&tail,10);if(*tail || port<1 || port>65535)return -1;
        long players=strtol(fields[4],&tail,10);if(*tail || players<0 || players>10000000)return -1;
        long slots=strtol(fields[5],&tail,10);if(*tail || slots<0 || slots>10000000)return -1;
        if(count<capacity) {SavedServer *s=&rows[count++];memset(s,0,sizeof(*s));strncpy(s->name,fields[1],sizeof(s->name)-1);strncpy(s->host,fields[2],sizeof(s->host)-1);s->port=(int)port;s->users_idle=(int)players;s->capacity=(int)slots;s->discovered=1;}
        line=end+1;
    }
    return count;
}
static int fetch_lan(void *unused){
    (void)unused;sock_t fd=socket(AF_INET,SOCK_DGRAM,0);
    if(fd==INVALID_SOCK){snprintf(job.error,sizeof(job.error),"Cannot start LAN discovery");SDL_AtomicSet(&job.done,1);return 0;}
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_BROADCAST,(const char*)&yes,sizeof(yes));net_set_nonblocking(fd);
    char nonce[17],query[40];snprintf(nonce,sizeof(nonce),"%016llx",(unsigned long long)SDL_GetPerformanceCounter());snprintf(query,sizeof(query),"LC-LAN-1 %s",nonce);
    struct sockaddr_in dest;memset(&dest,0,sizeof(dest));dest.sin_family=AF_INET;dest.sin_port=htons(31459);dest.sin_addr.s_addr=INADDR_BROADCAST;
    sendto(fd,query,(int)strlen(query),0,(struct sockaddr*)&dest,sizeof(dest));dest.sin_addr.s_addr=htonl(INADDR_LOOPBACK);sendto(fd,query,(int)strlen(query),0,(struct sockaddr*)&dest,sizeof(dest));
    snprintf(job.body,sizeof(job.body),"LC-DIRECTORY-1\n");job.used=strlen(job.body);
    Uint32 started=SDL_GetTicks();int count=0;
    while(SDL_GetTicks()-started<1800 && count<64){
        char reply[256],marker[24],echo[24],name[128];int port;struct sockaddr_in sender;socklen_t len=sizeof(sender);
        int n=recvfrom(fd,reply,sizeof(reply)-1,0,(struct sockaddr*)&sender,&len);
        if(n<=0){SDL_Delay(10);continue;}reply[n]=0;
        unsigned ip=ntohl(sender.sin_addr.s_addr);if(!((ip>>24)==10 || (ip>>24)==127 || (ip>>20)==0xac1 || (ip>>16)==0xc0a8 || (ip>>16)==0xa9fe))continue;
        if(sscanf(reply,"%23[^\t]\t%23[^\t]\t%d\t%127[^\r\n]",marker,echo,&port,name)!=4 || strcmp(marker,"LC-LAN-1") || strcmp(echo,nonce) || port<1 || port>65535)continue;
        for(char *p=name;*p;p++)if((unsigned char)*p<32)*p=' ';
        char line[384];snprintf(line,sizeof(line),"lan%d\t%s\t%s\t%d\t0\t0\n",count++,name,inet_ntoa(sender.sin_addr),port);
        size_t size=strlen(line);if(size>=sizeof(job.body)-job.used)break;memcpy(job.body+job.used,line,size+1);job.used+=size;
    }
    sock_close(fd);SDL_AtomicSet(&job.done,1);return 0;
}
int directory_start_lan(char *message,int length){
    if(job.thread){snprintf(message,length,"Discovery already running");return 0;}
    job.lan=1;job.used=0;job.body[0]=0;job.error[0]=0;SDL_AtomicSet(&job.done,0);
    job.thread=SDL_CreateThread(fetch_lan,"LAN discovery",NULL);snprintf(message,length,job.thread?"Finding local servers…":"Cannot start LAN discovery");return job.thread!=NULL;
}
int directory_is_lan(void){return job.lan;}
int directory_busy(void){return job.thread!=NULL;}
int directory_start(const char *url,char *message,int length) {
    if(job.thread){snprintf(message,length,"Discovery already running");return 0;}
    job.lan=0;
    const char *allow=SDL_getenv("LINECLASH_DIRECTORY_ALLOW_LOCAL");
    if(!directory_url_valid(url,allow && !strcmp(allow,"1"))){snprintf(message,length,"Public discovery is not configured. Add a server to get started.");return 0;}
#ifndef PLATFORM_WINDOWS
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return 0;
#endif
    memset(job.body,0,sizeof(job.body));job.used=0;job.error[0]=0;SDL_AtomicSet(&job.done,0);snprintf(job.url,sizeof(job.url),"%s",url);
    job.thread=SDL_CreateThread(fetch,"server directory",NULL);snprintf(message,length,"%s",job.thread?"Finding public servers…":"Cannot start discovery");return job.thread!=NULL;
}
int directory_poll(SavedServer *rows,int capacity,char *message,int length) {
    if(!job.thread || !SDL_AtomicGet(&job.done))return -2;
    SDL_WaitThread(job.thread,NULL);job.thread=NULL;
    int count=job.error[0]?-1:directory_parse(job.body,rows,capacity);
    if(count<0)snprintf(message,length,"%s",job.error[0]?job.error:"Invalid directory response; list unchanged");
    else snprintf(message,length,"Found %d %s servers.",count,job.lan?"local":"public");
    return count;
}
void directory_shutdown(void){if(job.thread){SDL_WaitThread(job.thread,NULL);job.thread=NULL;}}
