/* lineclash.c - LineClash client entry point */
#include "client.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>


int main(int argc, char *argv[]) {
    for (int i=1; i<argc; i++) {
        if(!strcmp(argv[i],"--smoke-test")){
            if(!net_init())return 1;
            if(!sdl_init()){net_cleanup();return 1;}
            int ok=sound_init();
            if(ok){music_set_res_prefix(sdl_get_res_dir());music_select(0);MusicPlayback playback;
                ok=0;for(int n=0;n<150;n++){music_tick();music_snapshot(&playback);if(playback.failed)break;if(playback.duration>0){ok=1;break;}SLEEP_MS(20);}
            }
            sound_shutdown();sdl_shutdown();net_cleanup();return ok?0:1;
        }
        if (!strcmp(argv[i],"--debug")) {
            /* Redirect stderr to debug log */
            char debug_path[1024];
#ifdef PLATFORM_WINDOWS
            char temp[MAX_PATH];
            DWORD n=GetTempPathA(sizeof(temp),temp);
            snprintf(debug_path,sizeof(debug_path),"%slineclash-gui-debug.log",n>0 && n<sizeof(temp)?temp:".\\");
#else
            snprintf(debug_path,sizeof(debug_path),"%s/lineclash-gui-debug.log",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
#endif
            FILE *dbg = fopen(debug_path,"w");
            if (dbg) { dup2(fileno(dbg), STDERR_FILENO); fclose(dbg); }
            fprintf(stderr,"lineclash-gui debug log\n"); fflush(stderr);
        }
    }
    client_run_gui();
    return 0;
}

#ifdef PLATFORM_WINDOWS
/* A native GUI executable launches without an extra console window. */
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show) {
    (void)instance;(void)previous;(void)command;(void)show;
    return main(__argc,__argv);
}
#endif
