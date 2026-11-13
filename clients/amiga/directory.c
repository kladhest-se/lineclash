/* Native AmiSSL public directory; gameplay remains ordinary TCP. */
#include "../gui/src/client.h"
#include <ctype.h>
#include <dos/dostags.h>
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
static int pending,lan_mode;static char reply_path[96],done_path[96],script_path[96];
int directory_is_lan(void){return lan_mode;}int directory_busy(void){return pending;}
int directory_url_valid(const char *url,int local){(void)local;return url && !strcmp(url,"https://lineclash.kladhest.se/api/");}
static int start_job(const char *url,char *message,int length,int lan){
 if(pending){snprintf(message,length,"Discovery already running");return 0;}if(!lan && !directory_url_valid(url,0)){snprintf(message,length,"Use the official HTTPS directory");return 0;}
 char directory[256],command[160];BPTR lock=GetProgramDir();if(!NameFromLock(lock,directory,sizeof(directory)) || strpbrk(directory,"\"*\r\n"))return 0;
 unsigned long id=(unsigned long)FindTask(NULL);snprintf(reply_path,sizeof(reply_path),"T:lineclash-%lx.reply",id);snprintf(done_path,sizeof(done_path),"T:lineclash-%lx.done",id);snprintf(script_path,sizeof(script_path),"T:lineclash-%lx.script",id);remove(done_path);remove(reply_path);
 FILE *file=fopen(script_path,"w");if(!file)return 0;
 fprintf(file,"FailAt 21\nStack 524288\n\"%s%sLineClash\" %s >%s\nIf WARN\nEcho FAIL >%s\nElse\nEcho OK >%s\nEndIf\n",directory,directory[strlen(directory)-1]==':'?"":"/",lan?"--lan-worker":"--directory-worker",reply_path,done_path,done_path);fclose(file);
 snprintf(command,sizeof(command),"C:Execute %s",script_path);if(SystemTags(command,SYS_Asynch,TRUE,TAG_DONE)==-1)return 0;
 pending=1;lan_mode=lan;snprintf(message,length,"Finding public servers with AmiSSL...");return 1;
}
int directory_start(const char *url,char *message,int length){return start_job(url,message,length,0);}
int directory_start_lan(char *message,int length){return start_job("",message,length,1);}
int directory_poll(SavedServer *rows,int capacity,char *message,int length){
 if(!pending)return -2;FILE *done=fopen(done_path,"r");if(!done)return -2;char result[16]={0};fgets(result,sizeof(result),done);fclose(done);pending=0;
 int count=-1;if(!strncmp(result,"OK",2)){FILE *file=fopen(reply_path,"rb");if(file){char *body=malloc(65537);if(body){size_t size=fread(body,1,65536,file);body[size]=0;char *feed=strstr(body,"\r\n\r\n");if(feed && (strstr(body,"HTTP/1.0 200")==body || strstr(body,"HTTP/1.1 200")==body))count=directory_parse(feed+4,rows,capacity);free(body);}fclose(file);}}
 remove(done_path);remove(reply_path);remove(script_path);snprintf(message,length,count>=0?"Public server list loaded":"HTTPS discovery failed: check AmiSSL, certificates and system date");return count;
}
void directory_shutdown(void){if(!pending){remove(done_path);remove(reply_path);remove(script_path);}}

int amiga_lan_worker(void){
 if(!net_init())return 20;int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0){net_cleanup();return 20;}
 int yes=1;setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));net_set_nonblocking(fd);
 char nonce[17],query[40];snprintf(nonce,sizeof(nonce),"%016llx",(unsigned long long)net_now_ms());snprintf(query,sizeof(query),"LC-LAN-1 %s",nonce);
 struct sockaddr_in destination;memset(&destination,0,sizeof(destination));destination.sin_len=sizeof(destination);destination.sin_family=AF_INET;destination.sin_port=htons(31459);destination.sin_addr.s_addr=INADDR_BROADCAST;
 sendto(fd,query,strlen(query),0,(struct sockaddr*)&destination,sizeof(destination));
 printf("HTTP/1.0 200 OK\r\n\r\nLC-DIRECTORY-1\n");uint64_t started=net_now_ms();int found=0;
 while(net_now_ms()-started<2000 && found<32){char buffer[256],marker[24],echo[24],name[128];int port;struct sockaddr_in sender;socklen_t size=sizeof(sender);int n=recvfrom(fd,buffer,sizeof(buffer)-1,0,(struct sockaddr*)&sender,&size);
  if(n<=0){Delay(1);continue;}buffer[n]=0;unsigned long ip=ntohl(sender.sin_addr.s_addr);
  if(!((ip>>24)==10 || (ip>>24)==127 || (ip>>20)==0xac1 || (ip>>16)==0xc0a8 || (ip>>16)==0xa9fe))continue;
  if(sscanf(buffer,"%23[^\t]\t%23[^\t]\t%d\t%127[^\r\n]",marker,echo,&port,name)!=4 || strcmp(marker,"LC-LAN-1") || strcmp(echo,nonce) || port<1 || port>65535)continue;
  for(char *p=name;*p;p++)if((unsigned char)*p<32)*p=' ';
  printf("lan%d\t%s\t%s\t%d\t0\t0\n",found++,name,inet_ntoa(sender.sin_addr),port);
 }
 sock_close(fd);net_cleanup();return fflush(stdout)==0?0:20;
}
