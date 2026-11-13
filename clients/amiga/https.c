/* AmiSSL 4+ HTTPS worker. Certificate and hostname checks remain enabled. */
#include "../gui/src/client.h"
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <amissl/amissl.h>
struct Library *AmiSSLBase,*AmiSSLMasterBase;
static int transfer(const char *body,char *reply,size_t capacity){
 SSL_CTX *ctx=0;SSL *ssl=0;sock_t fd=INVALID_SOCK;int initialized=0,result=20;
 const char *host="lineclash.kladhest.se";
 if(!net_init())return 20;
 AmiSSLMasterBase=OpenLibrary("amisslmaster.library",4);if(!AmiSSLMasterBase || !InitAmiSSLMaster(0x10,0))goto done;
 AmiSSLBase=OpenAmiSSL();if(!AmiSSLBase)goto done;
 struct TagItem tags[3]={{TAG_USER+1,(ULONG)SocketBase},{TAG_USER+11,0},{TAG_DONE,0}};int error=0;tags[1].ti_Data=(ULONG)&error;
 if(InitAmiSSLA(tags))goto done;initialized=1;
 ctx=SSL_CTX_new(TLS_client_method());if(!ctx || !SSL_CTX_load_verify_locations(ctx,NULL,"AmiSSL:Certs"))goto done;
 SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);ssl=SSL_new(ctx);if(!ssl || !SSL_set1_host(ssl,host) || !SSL_set_tlsext_host_name(ssl,host))goto done;
 const struct hostent *resolved=gethostbyname(host);if(!resolved)goto done;
 struct sockaddr_in peer;memset(&peer,0,sizeof(peer));peer.sin_len=sizeof(peer);peer.sin_family=AF_INET;peer.sin_port=htons(443);memcpy(&peer.sin_addr,resolved->h_addr,4);
 fd=socket(AF_INET,SOCK_STREAM,0);if(fd==INVALID_SOCK)goto done;
 struct timeval timeout={12,0};setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
 if(connect(fd,(struct sockaddr*)&peer,sizeof(peer)) || !SSL_set_fd(ssl,fd) || SSL_connect(ssl)!=1)goto done;
 char request[1400];
 if(body)snprintf(request,sizeof(request),"POST /api/ HTTP/1.0\r\nHost: lineclash.kladhest.se\r\nContent-Type: application/json\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n%s",(unsigned long)strlen(body),body);
 else snprintf(request,sizeof(request),"GET /api/?action=servers&format=text HTTP/1.0\r\nHost: lineclash.kladhest.se\r\nUser-Agent: LineClash-Amiga/1.0\r\nConnection: close\r\n\r\n");
 size_t sent=0;while(sent<strlen(request)){int n=SSL_write(ssl,request+sent,strlen(request)-sent);if(n<=0)goto done;sent+=n;}
 account_erase(request,sizeof(request));
 char *response=malloc(65537);if(!response)goto done;
 size_t total=0;int n=0;
 while(total<65536 && (n=SSL_read(ssl,response+total,65536-total))>0){if(memchr(response+total,0,n)){n=-1;break;}total+=n;}response[total]=0;
 char *content=strstr(response,"\r\n\r\n");int status=0;sscanf(response,"HTTP/%*s %d",&status);
 if(n==0 && content){
  if(reply){size_t length=total-(size_t)(content+4-response);if((status==200 || status==401 || status==403 || status==409) && length<capacity){memcpy(reply,content+4,length+1);result=0;}}
  else if(status==200 && fwrite(response,1,total,stdout)==total && fflush(stdout)==0)result=0;
 }
 account_erase(response,total);free(response);
 done:if(ssl)SSL_free(ssl);if(fd!=INVALID_SOCK)sock_close(fd);if(ctx)SSL_CTX_free(ctx);if(initialized)CleanupAmiSSLA(NULL);if(AmiSSLBase)CloseAmiSSL();if(AmiSSLMasterBase)CloseLibrary(AmiSSLMasterBase);net_cleanup();return result;
}

int amiga_directory_worker(void){return transfer(NULL,NULL,0);}
int amiga_https_request(const char *body,char *reply,size_t size){return transfer(body,reply,size)==0;}
