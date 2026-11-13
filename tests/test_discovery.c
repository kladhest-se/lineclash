#include "../clients/gui/src/client.h"
#include <SDL.h>
#include <assert.h>
int main(int argc,char **argv){assert(argc==2);assert(SDL_Init(0)==0);SDL_setenv("LINECLASH_DIRECTORY_ALLOW_LOCAL","1",1);SavedServer rows[64];char message[128];assert(directory_start(argv[1],message,sizeof(message)));int count=-2;for(int i=0;i<1000 && count==-2;i++){count=directory_poll(rows,64,message,sizeof(message));SDL_Delay(10);}assert(count==1);assert(!strcmp(rows[0].host,"127.0.0.1") && rows[0].capacity==8);assert(!rows[0].pw_hash[0]);directory_shutdown();SDL_Quit();return 0;}
