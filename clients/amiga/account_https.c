/* Worker IPC keeps passwords and sessions out of files and command-line arguments. */
#include "../gui/src/client.h"
#include <exec/memory.h>
#include <dos/dostags.h>
#include <proto/exec.h>
static struct MsgPort *requests,*replies;
static struct AccountMessage {struct Message message;char body[1024],reply[512];int ok;} *job;
static char port_name[64];
int amiga_https_request(const char *,char *,size_t);
int account_https_busy(void){return job!=NULL;}
static void dispose(void){if(requests){RemPort(requests);DeleteMsgPort(requests);requests=NULL;}if(replies){DeleteMsgPort(replies);replies=NULL;}if(job){account_erase(job,sizeof(*job));FreeMem(job,sizeof(*job));job=NULL;}}
int account_https_start(const char *body){
 if(job || strlen(body)>=1024)return 0;
 replies=CreateMsgPort();requests=CreateMsgPort();job=AllocMem(sizeof(*job),MEMF_PUBLIC|MEMF_CLEAR);
 if(!requests || !replies || !job){if(requests)DeleteMsgPort(requests);if(replies)DeleteMsgPort(replies);if(job)FreeMem(job,sizeof(*job));requests=replies=NULL;job=NULL;return 0;}
 snprintf(port_name,sizeof(port_name),"LineClashAuth-%lx",(unsigned long)FindTask(NULL));requests->mp_Node.ln_Name=port_name;AddPort(requests);
 job->message.mn_ReplyPort=replies;job->message.mn_Length=sizeof(*job);snprintf(job->body,sizeof(job->body),"%s",body);PutMsg(requests,&job->message);
 BPTR segment=LoadSeg("PROGDIR:LineClash");if(!segment){GetMsg(requests);dispose();return 0;}
 char args[128];snprintf(args,sizeof(args),"--account-worker %s\n",port_name);
 struct Process *worker=CreateNewProcTags(NP_Seglist,(ULONG)segment,NP_FreeSeglist,TRUE,NP_StackSize,524288,NP_Cli,TRUE,NP_CommandName,(ULONG)"LineClash",NP_Arguments,(ULONG)args,TAG_DONE);
 if(!worker){UnLoadSeg(segment);GetMsg(requests);dispose();return 0;}return 1;
}
int account_https_poll(char *out,int size){if(!job || !GetMsg(replies))return 0;snprintf(out,size,"%s",job->reply);int result=job->ok?1:-1;dispose();return result;}
void account_https_shutdown(void){if(job){WaitPort(replies);GetMsg(replies);dispose();}}
int amiga_account_worker(const char *name){
 Forbid();struct MsgPort *port=FindPort((STRPTR)name);struct AccountMessage *request=port?(struct AccountMessage*)GetMsg(port):NULL;Permit();
 if(!request)return 20;
 request->ok=amiga_https_request(request->body,request->reply,sizeof(request->reply));account_erase(request->body,sizeof(request->body));ReplyMsg(&request->message);return 0;
}
