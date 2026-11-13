#include "server_internal.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
static sock_t lan_socket=INVALID_SOCK;
void server_lan_start(void){
    if(!g_cfg.lan_discovery)return;
    lan_socket=socket(AF_INET,SOCK_DGRAM,0);if(lan_socket==INVALID_SOCK)return;
    struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_port=htons(31459);address.sin_addr.s_addr=INADDR_ANY;
    if(bind(lan_socket,(struct sockaddr*)&address,sizeof(address))<0){sock_close(lan_socket);lan_socket=INVALID_SOCK;log_info("LAN discovery unavailable on UDP 31459; direct connections still work");return;}
    net_set_nonblocking(lan_socket);
}
void server_lan_tick(void){
    if(lan_socket==INVALID_SOCK)return;
    for(int i=0;i<8;i++){
        char query[96];struct sockaddr_in sender;socklen_t len=sizeof(sender);
        int n=recvfrom(lan_socket,query,sizeof(query)-1,0,(struct sockaddr*)&sender,&len);if(n<=0)break;query[n]=0;
        unsigned ip=ntohl(sender.sin_addr.s_addr);
        if(!((ip>>24)==10 || (ip>>24)==127 || (ip>>20)==0xac1 || (ip>>16)==0xc0a8 || (ip>>16)==0xa9fe))continue;
        char nonce[17];if(n!=25 || strncmp(query,"LC-LAN-1 ",9))continue;
        memcpy(nonce,query+9,16);nonce[16]=0;if(strspn(nonce,"0123456789abcdef")!=16)continue;
        char name[128];snprintf(name,sizeof(name),"%s",g_cfg.server_name);for(char *p=name;*p;p++)if((unsigned char)*p<32)*p=' ';
        char reply[240];snprintf(reply,sizeof(reply),"LC-LAN-1\t%s\t%d\t%s",nonce,g_cfg.port,name);
        sendto(lan_socket,reply,(int)strlen(reply),0,(struct sockaddr*)&sender,len);
    }
}
void server_lan_stop(void){if(lan_socket!=INVALID_SOCK)sock_close(lan_socket);lan_socket=INVALID_SOCK;}
