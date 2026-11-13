#include "../gui/src/client.h"
unsigned long __stack=524288;
int amiga_directory_worker(void);int amiga_lan_worker(void);
int main(int argc,char **argv){if(argc>1 && !strcmp(argv[1],"--directory-worker"))return amiga_directory_worker();if(argc>1 && !strcmp(argv[1],"--lan-worker"))return amiga_lan_worker();FILE *log=fopen("PROGDIR:LineClash.log","a");if(log)log_set_file(log);log_info("Amiga client starting");client_run_gui();log_info("Amiga client stopped");log_set_file(NULL);if(log)fclose(log);return 0;}
