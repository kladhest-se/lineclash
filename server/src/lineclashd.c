/* lineclashd.c - LineClash server main */
#include "../../shared/include/lineclash.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef PLATFORM_POSIX
#include <sys/stat.h>
#endif

/* Implemented in server.c */
void server_run(int port, const char *cfg_path);

#ifdef PLATFORM_AMIGA
struct Library *SocketBase = NULL;
#endif

#ifndef PLATFORM_WINDOWS
static void handle_sigint(int sig) {
    (void)sig;
    exit(0);
}
#endif

/* Ensure a directory path exists (create parent dirs) */
static void ensure_dir(const char *filepath) {
#ifdef PLATFORM_POSIX
    char dir[CFG_MAX_PATH];
    strncpy(dir, filepath, CFG_MAX_PATH-1); dir[CFG_MAX_PATH-1]='\0';
    char *sl = strrchr(dir, '/');
    if (sl && sl != dir) {
        *sl = '\0';
        char cmd[CFG_MAX_PATH+16];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
        { int _r=system(cmd); (void)_r; }
    }
#elif defined(PLATFORM_WINDOWS)
    char dir[CFG_MAX_PATH];snprintf(dir,sizeof(dir),"%s",filepath);
    for(char *p=dir;*p;p++)if((*p=='/' || *p=='\\') && p>dir && p[-1]!=':') {
        char saved=*p;*p='\0';CreateDirectoryA(dir,NULL);*p=saved;
    }
#else
    (void)filepath;
#endif
}

static void print_version(void) {
    printf("LineClash Server v%s  (%s)\n",
           TETRINETZ_VERSION, BUILD_DATE);
}

static void usage(const char *prog) {
    print_version();
    printf("\nUsage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --config <path>    Path to config file (default: %s)\n", CFG_FILE);
    printf("  --port   <port>    TCP port override    (default: %d)\n", DEFAULT_PORT);
    printf("  --logfile <path>   Log file override    (default: %s)\n", CFG_DEFAULT_LOG);
    printf("  -v, --version      Show version and exit\n");
    printf("  -h, --help         Show this help\n\n");
    printf("Default paths:\n");
    printf("  Config  : %s\n", CFG_FILE);
    printf("  Database: %s\n", CFG_DEFAULT_DB);
    printf("  Log     : %s\n\n", CFG_DEFAULT_LOG);
    printf("Console commands: type 'help' or '/help' for list\n");
}

int main(int argc, char *argv[]) {
    int   port     = DEFAULT_PORT;
    int   port_set = 0;
    char  cfg_path[CFG_MAX_PATH] = "";
    char  logfile_override[CFG_MAX_PATH] = "";
    int   i;

#ifdef PLATFORM_AMIGA
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) { fprintf(stderr, "Cannot open bsdsocket.library\n"); return 1; }
#endif

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]); return 0;
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            print_version(); return 0;
        } else if (!strcmp(argv[i], "--config") && i+1 < argc) {
            strncpy(cfg_path, argv[++i], CFG_MAX_PATH-1);
        } else if (!strcmp(argv[i], "--port") && i+1 < argc) {
            port = atoi(argv[++i]);
            port_set = 1;
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Invalid port: %s\n", argv[i]);
                return 1;
            }
        } else if (!strcmp(argv[i], "--logfile") && i+1 < argc) {
            strncpy(logfile_override, argv[++i], CFG_MAX_PATH-1);
        } else if (argv[i][0] != '-') {
            port = atoi(argv[i]);
            port_set = 1;
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Invalid port: %s\n", argv[i]);
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]); return 1;
        }
    }

    /* Load config early so we can open log before first output */
    config_load(&g_cfg, cfg_path[0] ? cfg_path : NULL);
    if (port_set)            g_cfg.port = port;
    if (logfile_override[0]) strncpy(g_cfg.log_file, logfile_override, CFG_MAX_PATH-1);

    /* Ensure parent directories exist for log and db files */
    ensure_dir(g_cfg.log_file);
    ensure_dir(g_cfg.db_file);

    /* Open log file before the first log_info call */
    if (g_cfg.log_file[0]) {
        FILE *lf = fopen(g_cfg.log_file, "a");
        if (lf) log_set_file(lf);
        else fprintf(stderr, "Warning: cannot open log file '%s'\n", g_cfg.log_file);
    }

#ifndef PLATFORM_WINDOWS
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);
#endif

    server_run(g_cfg.port, cfg_path[0] ? cfg_path : NULL);

#ifdef PLATFORM_AMIGA
    if (SocketBase) CloseLibrary(SocketBase);
#endif
    return 0;
}
