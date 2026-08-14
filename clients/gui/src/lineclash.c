/* lineclash.c - LineClash client entry point */
#include "client.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>


int main(int argc, char *argv[]) {
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"--debug")) {
            /* Redirect stderr to debug log */
            FILE *dbg = fopen("/tmp/lineclash-gui-debug.log","w");
            if (dbg) { dup2(fileno(dbg), STDERR_FILENO); fclose(dbg); }
            fprintf(stderr,"lineclash-gui debug log\n"); fflush(stderr);
        }
    }
    client_run_gui();
    return 0;
}
