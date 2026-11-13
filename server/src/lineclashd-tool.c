/*
 * lineclashd-tool — LineClash Server Admin Tool
 *
 * Credentials file: ~/.lineclashd-tool-secret  (override with --cfg <path>)
 *   [alias]
 *   host = hostname
 *   port = 31457
 *   nick = adminuser
 *   hash = <sha256hex of password>
 *
 * Usage:
 *   lineclashd-tool --add --host <host> [--port <p>] --username <u> --password <pw>
 *   lineclashd-tool --remove <alias>
 *   lineclashd-tool --rename <alias> <new-alias>
 *   lineclashd-tool --status <alias>
 *   lineclashd-tool --list
 *   lineclashd-tool --show <alias>
 *   lineclashd-tool --connect <alias>
 *   lineclashd-tool --command <alias> <command>  [--debug]
 *   lineclashd-tool --version
 *   lineclashd-tool --help
 */

#include "../../shared/include/lineclash.h"
#include "auth.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef _WIN32
#include <termios.h>
#endif

/* Shared buffering drains complete lines before reading more network data. */
#define TOOL_RECV_BUF RECV_BUF_SIZE
static char g_auth_hash[65];
static int tool_net_recv(sock_t s, char *buf, int *len, char *line, int maxline) {
    return net_recv(s, buf, len, line, maxline);
}

/* Password masking removed — tool no longer prompts interactively */

/* ================================================================ Credentials */
#define CREDS_MAX 32
typedef struct { char alias[64]; char host[256]; int port;
                 char nick[MAX_NICK_LEN]; char hash[65]; } Profile;
static Profile g_profiles[CREDS_MAX];
static int     g_nprofiles = 0;

static char g_creds_path_override[512] = "";

static const char *creds_path(void) {
    if (g_creds_path_override[0]) return g_creds_path_override;
    static char path[512];
#if defined(PLATFORM_WINDOWS)
    const char *d = getenv("APPDATA");
    snprintf(path,sizeof(path),"%s\\LineClash\\lineclashd-tool-secret",d?d:".");
#elif defined(PLATFORM_AMIGA)
    snprintf(path,sizeof(path),"ENVARC:LineClash/lineclashd-tool-secret");
#else
    const char *home = getenv("HOME");
    snprintf(path,sizeof(path),"%s/.lineclashd-tool-secret",home?home:".");
#endif
    return path;
}

static void creds_load(void) {
    g_nprofiles = 0;
    FILE *f = fopen(creds_path(),"r"); if (!f) return;
    char line[512]; Profile *cur=NULL;
    while (fgets(line,sizeof(line),f)) {
        char *nl=strchr(line,'\n'); if(nl)*nl='\0';
        char *cr=strchr(line,'\r'); if(cr)*cr='\0';
        if (line[0]=='[') {
            if (g_nprofiles<CREDS_MAX) {
                cur=&g_profiles[g_nprofiles++]; memset(cur,0,sizeof(*cur)); cur->port=DEFAULT_PORT;
                size_t len=strlen(line); if(len>1&&line[len-1]==']')line[len-1]='\0';
                strncpy(cur->alias,line+1,sizeof(cur->alias)-1);
            }
        } else if (cur) {
            char key[64],val[256];
            if (sscanf(line," %63[^=] = %255[^\n]",key,val)==2) {
                int kl=(int)strlen(key); while(kl>0&&key[kl-1]==' ')key[--kl]='\0';
                if(!strcmp(key,"host"))      strncpy(cur->host,val,sizeof(cur->host)-1);
                else if(!strcmp(key,"port")) cur->port=atoi(val);
                else if(!strcmp(key,"nick")) strncpy(cur->nick,val,sizeof(cur->nick)-1);
                else if(!strcmp(key,"hash")) strncpy(cur->hash,val,sizeof(cur->hash)-1);
            }
        }
    }
    fclose(f);
}

static int creds_write(void) {
    FILE *f=fopen(creds_path(),"w");
    if(!f){fprintf(stderr,"Cannot write to %s\n",creds_path());return 0;}
    int i;
    for(i=0;i<g_nprofiles;i++){
        Profile *p=&g_profiles[i]; if(!p->alias[0])continue;
        fprintf(f,"[%s]\nhost = %s\nport = %d\nnick = %s\n",
                p->alias,p->host,p->port,p->nick);
        if(p->hash[0]) fprintf(f,"hash = %s\n",p->hash);
        fprintf(f,"\n");
    }
    fclose(f); return 1;
}

static Profile *creds_find(const char *alias) {
    int i; for(i=0;i<g_nprofiles;i++) if(!strcasecmp(g_profiles[i].alias,alias))return&g_profiles[i];
    return NULL;
}

static Profile *creds_upsert(const char *alias,const char *host,int port,
                              const char *nick,const char *hash) {
    Profile *p=creds_find(alias);
    if(!p){
        if(g_nprofiles>=CREDS_MAX){fprintf(stderr,"Profile limit reached\n");return NULL;}
        p=&g_profiles[g_nprofiles++]; memset(p,0,sizeof(*p));
    }
    strncpy(p->alias,alias,sizeof(p->alias)-1); strncpy(p->host,host,sizeof(p->host)-1);
    p->port=port; strncpy(p->nick,nick,sizeof(p->nick)-1); strncpy(p->hash,hash,sizeof(p->hash)-1);
    return p;
}

/* ================================================================ Version / help */
static void print_version(void){printf("LineClash Admin Tool v%s  (%s)\n",TETRINETZ_VERSION,BUILD_DATE);}

static void usage(const char *prog) {
    print_version();
    printf("\nUsage:\n");
    printf("  %s --add --host <host> [--port <port>] --username <user> --password <pw>\n",prog);
    printf("                              Add a server profile (password stored as hash)\n");
    printf("  %s --list                   List all saved server profiles\n",prog);
    printf("  %s --show <alias>           Show details of a saved profile\n",prog);
    printf("  %s --remove <alias>         Remove a saved server profile\n",prog);
    printf("  %s --rename <alias> <new>   Rename a saved server profile\n",prog);
    printf("  %s --status <alias>         Test connection and authentication\n",prog);
    printf("  %s --connect <alias>        Connect and open interactive prompt\n",prog);
    printf("  %s --command <alias> <cmd>  Run a single command, print output, exit\n",prog);
    printf("\nOptions:\n");
    printf("  --debug                     Show connection/auth progress (no secrets)\n");
    printf("  --quiet                     Suppress connection/auth status messages\n");
    printf("  --cfg <path>                Use a custom credentials file\n");
    printf("  --version                   Show version\n");
    printf("  --help                      Show this help\n");
    printf("\nCredentials file: %s\n", creds_path());
    printf("Once connected, type 'help' for server commands, 'quit' to disconnect.\n");
}



static int cmd_rename(const char *a, const char *b) {
    Profile *p = creds_find(a);
    if (!p) { fprintf(stderr,"Profile '%s' not found.\n",a); return 1; }
    if (creds_find(b)) { fprintf(stderr,"Profile '%s' already exists.\n",b); return 1; }
    strncpy(p->alias, b, sizeof(p->alias)-1);
    if (creds_write()) printf("Renamed '%s' -> '%s'\n", a, b);
    return 0;
}

static int cmd_delete(const char *alias) {
    int i;
    for (i = 0; i < g_nprofiles; i++) {
        if (!strcasecmp(g_profiles[i].alias, alias)) {
            memmove(&g_profiles[i], &g_profiles[i+1],
                    (g_nprofiles-i-1)*sizeof(Profile));
            g_nprofiles--;
            if (creds_write()) printf("Removed profile '%s'\n", alias);
            return 0;
        }
    }
    fprintf(stderr,"Profile '%s' not found.\n", alias);
    return 1;
}

/* ================================================================ Connect + auth */
static sock_t do_connect_auth(const char *host, int port, const char *nick,
                               const char *hash_in, char *srvname_out, int sn_len,
                               char *recv_buf, int *recv_len, int debug, int quiet) {
    memset(g_auth_hash, 0, sizeof(g_auth_hash));
    if (hash_in && hash_in[0]) memcpy(g_auth_hash, hash_in, 64);
    const char *hash = g_auth_hash;
    char hmac[65], buf[MAX_MSG_LEN], line2[MAX_MSG_LEN];
    if (debug) fprintf(stderr,"[AUTH-DEBUG] Starting authentication\n");
    if (srvname_out && sn_len > 0) srvname_out[0] = '\0';

    if (!quiet) printf("Connecting to %s:%d ...\n", host, port);
    net_init();
    sock_t s = net_connect(host, port);
    if (s == INVALID_SOCK) {
        fprintf(stderr,"Cannot connect to %s:%d\n", host, port);
        return INVALID_SOCK;
    }
    net_send(s, "toolmode");
    if (!quiet) printf("Connected. Authenticating as '%s'...\n", nick);

    int authed = 0;
    while (!authed) {
        if (*recv_len == 0) {
            fd_set rfds; struct timeval tv;
            FD_ZERO(&rfds); FD_SET(s,&rfds);
            tv.tv_sec=3; tv.tv_usec=0;
            int sel = select((int)(s+1), &rfds, NULL, NULL, &tv);
            if (sel < 0) { fprintf(stderr,"Select error during auth.\n"); sock_close(s); return INVALID_SOCK; }
            if (sel == 0) { fprintf(stderr,"Auth timed out.\n"); sock_close(s); return INVALID_SOCK; }
        }
        int r = tool_net_recv(s, recv_buf, recv_len, line2, sizeof(line2));
        if (r < 0) { fprintf(stderr,"Connection lost during auth.\n"); sock_close(s); return INVALID_SOCK; }
        if (r == 0) {
            fd_set rfds2; struct timeval tv2;
            FD_ZERO(&rfds2); FD_SET(s,&rfds2);
            tv2.tv_sec=3; tv2.tv_usec=0;
            int sel2 = select((int)(s+1), &rfds2, NULL, NULL, &tv2);
            if (sel2 <= 0) { fprintf(stderr,"Auth timed out.\n"); sock_close(s); return INVALID_SOCK; }
            continue;
        }
        char cmd[64], args[4][MAX_MSG_LEN];
        proto_parse(line2, cmd, args, 4);
        if (!strcmp(cmd,"servername") && srvname_out && args[0][0])
            strncpy(srvname_out, args[0], sn_len-1);
        else if (!strcmp(cmd,"lobbylistend")) {
            snprintf(buf, sizeof(buf), "authnick %s", nick);
            net_send(s, buf);
        } else if (!strcmp(cmd,"authchallenge")) {
            if (!strcmp(args[0],"new")) {
                snprintf(buf, sizeof(buf), "authresp %s", hash);
            } else {
                unsigned char key[32]; int ki;
                for (ki=0; ki<32; ki++) {
                    unsigned int b=0;
                    sscanf(hash+ki*2, "%02x", &b);
                    key[ki] = (unsigned char)b;
                }
                hmac_sha256_hex(key, args[0], hmac);
                snprintf(buf, sizeof(buf), "authresp %s", hmac);
                if (debug) fprintf(stderr,"[TOOL] Challenge response prepared\n");
            }
            net_send(s, buf);
        } else if (!strcmp(cmd,"authok")) {
            authed = 1;
        } else if (!strcmp(cmd,"autherr")) {
            const char *errmsg = line2;
            if (strncmp(errmsg,"autherr ",8)==0) errmsg += 8;
            fprintf(stderr,"Auth failed: %s\n", errmsg);
            sock_close(s); net_cleanup(); return INVALID_SOCK;
        }
    }
    net_set_nonblocking(s);
    if (!quiet) printf("Authenticated.\n");
    return s;
}

void run_monitor(sock_t s, char *rb, int *rl);  /* forward decl */
/* ================================================================ Run single command */
static void run_single(sock_t s, char *rb, int *rl, const char *cmd_str) {
    char buf[MAX_MSG_LEN], line2[MAX_MSG_LEN];
    const char *verb = (cmd_str[0]=='/') ? cmd_str+1 : cmd_str;
    int _is_sta = (!strncasecmp(verb,"server status",13)||!strcasecmp(verb,"status")||
                   !strncasecmp(verb,"server monitor",14)||!strcasecmp(verb,"monitor"));
    if (_is_sta) {
        snprintf(buf, sizeof(buf), "remoteadmin /server status"); net_send(s, buf);
        int _done=0,_wait=0; char _ln[MAX_MSG_LEN];
        while(!_done&&_wait<5000){
            int _r=net_recv(s,rb,rl,_ln,sizeof(_ln));
            if(_r<0)break;
            if(_r==0){fd_set _rf;struct timeval _tv;FD_ZERO(&_rf);FD_SET(s,&_rf);
                _tv.tv_sec=0;_tv.tv_usec=100000;
                if(select((int)(s+1),&_rf,NULL,NULL,&_tv)<=0){_wait+=100;continue;}_wait=0;continue;}
            _wait=0;
            char _c[64],_a[2][MAX_MSG_LEN];proto_parse(_ln,_c,_a,2);
            if(!strcmp(_c,"remoteadmin_resp")){const char*_p=_ln+16;if(*_p==' ')_p++;printf("%s\n",_p);if(!_p[0])_done=1;}
            else if(!strcmp(_c,"remoteadmin_err")){const char*_p=_ln+15;if(*_p==' ')_p++;fprintf(stderr,"[error] %s\n",_p);_done=1;}
        }
        return;
    }
    /* Send full command string (multi-word like "bot add 1 easy" need all tokens) */
    { const char *_full = (cmd_str[0]=='/') ? cmd_str+1 : cmd_str;
      snprintf(buf, sizeof(buf), "remoteadmin /%s", _full); }
    net_send(s, buf);
    int done=0, waited=0;
    while (!done && waited < 5000) {
        int r = net_recv(s, rb, rl, line2, sizeof(line2));
        if (r < 0) break;
        if (r == 0) {
            fd_set rfs; struct timeval tv;
            FD_ZERO(&rfs); FD_SET(s,&rfs);
            tv.tv_sec=0; tv.tv_usec=100000;
            int sel = select((int)(s+1), &rfs, NULL, NULL, &tv);
            if (sel <= 0) { waited += 100; continue; }
            waited = 0; continue;
        }
        waited = 0;
        char cmd[64], args[2][MAX_MSG_LEN];
        proto_parse(line2, cmd, args, 2);
        if (!strcmp(cmd,"remoteadmin_resp")) {
            const char *pay = line2+16; if (*pay==' ') pay++;
            printf("%s\n", pay); if (!pay[0]) done=1;
        } else if (!strcmp(cmd,"remoteadmin_err")) {
            const char *pay = line2+15; if (*pay==' ') pay++;
            fprintf(stderr,"[error] %s\n", pay); done=1;
        }
    }
}

/* ================================================================ Interactive prompt */
void run_monitor(sock_t s, char *rb, int *rl) {
    char line2[MAX_MSG_LEN];
    int running = 1;
    struct timespec last_refresh = {0,0};

    /* Put terminal into raw mode so q/r are immediate (no Enter needed) */
#ifndef _WIN32
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(unsigned int)(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif

    printf("\033[?25l\033[2J"); /* hide cursor, clear screen */
    fflush(stdout);

    while (running) {
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec  - last_refresh.tv_sec )*1000
                        + (now.tv_nsec - last_refresh.tv_nsec)/1000000;

        if (elapsed_ms >= 500 || last_refresh.tv_sec == 0) {
            last_refresh = now;
            /* Request fresh stats */
            net_send(s, "remoteadmin /server status");

            /* Collect response lines (wait up to 400ms) */
            printf("\033[H"); /* cursor home */
            struct timespec deadline_ts;
            clock_gettime(CLOCK_MONOTONIC, &deadline_ts);
            deadline_ts.tv_nsec += 400000000L;
            if (deadline_ts.tv_nsec >= 1000000000L) {
                deadline_ts.tv_sec++; deadline_ts.tv_nsec -= 1000000000L;
            }
            for (;;) {
                int r = net_recv(s, rb, rl, line2, sizeof(line2));
                if (r == 1) {
                    if (!strncmp(line2,"remoteadmin_resp",16)) {
                        const char *p = line2+16; if (*p==' ') p++;
                        printf("\033[K%s\n", p);
                    } else if (!strncmp(line2,"noconnecting",12)||r<0) {
                        running=0; break;
                    }
                } else {
                    /* Check if deadline passed */
                    struct timespec chk; clock_gettime(CLOCK_MONOTONIC,&chk);
                    if (chk.tv_sec > deadline_ts.tv_sec ||
                        (chk.tv_sec==deadline_ts.tv_sec && chk.tv_nsec>=deadline_ts.tv_nsec))
                        break;
                    /* Brief sleep and try again */
                    struct timespec tiny={0,5000000L}; nanosleep(&tiny,NULL);
                }
            }
            printf("\033[K\n\033[K  [monitor]  q = quit   r = refresh now\n");
            fflush(stdout);
        }

        /* Non-blocking keyread */
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch=='q'||ch=='Q') running=0;
            else if (ch=='r'||ch=='R') last_refresh.tv_sec=0;
        }

        /* Check connection health */
        {
            int r;
            while ((r = net_recv(s, rb, rl, line2, sizeof(line2))) == 1) {
                if (!strncmp(line2,"noconnecting",12)) { running=0; break; }
            }
            if (r < 0) running=0;
        }

        struct timespec sl={0,50000000L}; nanosleep(&sl,NULL);
    }

#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig); /* restore terminal */
#endif
    printf("\033[?25h\033[2J\033[H"); /* show cursor, clear */
    fflush(stdout);
}

static void run_interactive(sock_t s, char *rb, int *rl, int quiet) {
    char buf[MAX_MSG_LEN], line2[MAX_MSG_LEN];
    if (!quiet) printf("Type 'help' for commands. Type 'quit' to disconnect.\n\n");
    int running = 1;
    while (running) {
        int r;
        while ((r = net_recv(s, rb, rl, line2, sizeof(line2))) == 1) {
            char cmd[64], args[4][MAX_MSG_LEN];
            proto_parse(line2, cmd, args, 4);
            if (!strcmp(cmd,"remoteadmin_resp")) {
                const char *p = line2+16; if (*p==' ') p++;
                printf("%s\n", p);
            } else if (!strcmp(cmd,"remoteadmin_err")) {
                const char *p = line2+15; if (*p==' ') p++;
                printf("[error] %s\n", p);
            } else if (!strcmp(cmd,"noconnecting")) {
                printf("[server] %s\n", args[0]); running=0; break;
            }
        }
        if (r < 0) { printf("\nDisconnected.\n"); break; }
        if (!running) break;
        fd_set rfds; struct timeval tv;
        FD_ZERO(&rfds); FD_SET(0,&rfds); FD_SET(s,&rfds);
        tv.tv_sec=0; tv.tv_usec=100000;
        if (select((int)(s+1), &rfds, NULL, NULL, &tv) < 0) break;
        if (FD_ISSET(0, &rfds)) {
            char input[512];
            if (!fgets(input, sizeof(input), stdin)) break;
            { char *nl = strchr(input,'\n'); if (nl) *nl='\0'; }
            if (!input[0]) continue;
            if (!strcmp(input,"quit") || !strcmp(input,"/quit") ||
                !strcmp(input,"exit") || !strcmp(input,"/exit")) {
                net_send(s, "remoteadmin quit"); break;
            }
            const char *verb = (input[0]=='/') ? input+1 : input;
            int _sta2 = (!strncasecmp(verb,"server status",13)||!strcasecmp(verb,"status")||
                         !strncasecmp(verb,"server monitor",14)||!strcasecmp(verb,"monitor"));
            if (_sta2) {
                snprintf(buf, sizeof(buf), "remoteadmin /server status");
                net_send(s, buf);
                continue;
            }
            snprintf(buf, sizeof(buf), "remoteadmin /%s", verb);
            net_send(s, buf);
        }
    }
}

/* ================================================================ Helper: resolve profile */
static int resolve_profile(const char *alias, const char **host_out, int *port_out,
                            char *nick_out, char *hash_out) {
    Profile *p = creds_find(alias);
    if (!p) {
        fprintf(stderr,"Profile '%s' not found. Use --add to add it first.\n", alias);
        return 0;
    }
    *host_out = p->host;
    *port_out = p->port;
    strncpy(nick_out, p->nick, MAX_NICK_LEN-1);
    strncpy(hash_out, p->hash, 64); hash_out[64]='\0';
    return 1;
}

/* ================================================================ main */
int main(int argc, char *argv[]) {
    /* ---- Pre-scan for --cfg so creds_path() is correct before creds_load() ---- */
    int i;
    for (i = 1; i < argc-1; i++) {
        if (!strcmp(argv[i],"--cfg")) {
            strncpy(g_creds_path_override, argv[i+1], sizeof(g_creds_path_override)-1);
            break;
        }
    }

    creds_load();
    config_load(&g_cfg, NULL);

    if (argc < 2) { usage(argv[0]); return 0; }

    /* Apply --quiet early so log_info is suppressed before any connection */
    { int qi; for(qi=1;qi<argc;qi++) if(!strcmp(argv[qi],"--quiet")) log_set_quiet(1); }

    /* ---- Parse ---- */
    const char *opt_add_host     = NULL;
    const char *opt_add_user     = NULL;
    const char *opt_add_pass     = NULL;
    int         opt_add_port     = DEFAULT_PORT;
    int         opt_list         = 0;
    const char *opt_show         = NULL;
    const char *opt_remove       = NULL;
    const char *opt_rename_old   = NULL;
    const char *opt_rename_new   = NULL;
    const char *opt_status       = NULL;
    const char *opt_connect      = NULL;
    const char *opt_cmd_alias    = NULL;
    const char *opt_cmd_str      = NULL;
    int         opt_debug        = 0;
    int         opt_quiet        = 0;
    int         opt_do_add       = 0;
    int         opt_version      = 0;
    int         opt_help         = 0;

    for (i = 1; i < argc; i++) {
        if      (!strcmp(argv[i],"--help")    || !strcmp(argv[i],"-h")) opt_help=1;
        else if (!strcmp(argv[i],"--version") || !strcmp(argv[i],"-v")) opt_version=1;
        else if (!strcmp(argv[i],"--debug"))   opt_debug=1;
        else if (!strcmp(argv[i],"--quiet"))   opt_quiet=1;
        else if (!strcmp(argv[i],"--cfg") && i+1<argc)   i++; /* already consumed */
        else if (!strcmp(argv[i],"--add"))     opt_do_add=1;
        else if (!strcmp(argv[i],"--host")   && i+1<argc) opt_add_host=argv[++i];
        else if (!strcmp(argv[i],"--port")   && i+1<argc) opt_add_port=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--username")&& i+1<argc) opt_add_user=argv[++i];
        else if (!strcmp(argv[i],"--password")&& i+1<argc) opt_add_pass=argv[++i];
        else if (!strcmp(argv[i],"--list"))               opt_list=1;
        else if (!strcmp(argv[i],"--show")  && i+1<argc) opt_show=argv[++i];
        else if (!strcmp(argv[i],"--remove") && i+1<argc) opt_remove=argv[++i];
        else if (!strcmp(argv[i],"--rename") && i+2<argc) {
            opt_rename_old=argv[++i]; opt_rename_new=argv[++i];
        }
        else if (!strcmp(argv[i],"--status") && i+1<argc) opt_status=argv[++i];
        else if (!strcmp(argv[i],"--connect")&& i+1<argc) opt_connect=argv[++i];
        else if (!strcmp(argv[i],"--command")&& i+2<argc) {
            opt_cmd_alias=argv[++i]; opt_cmd_str=argv[++i];
        }
        else {
            fprintf(stderr,"Unknown option: %s\n", argv[i]);
            usage(argv[0]); return 1;
        }
    }

    if (opt_version) { print_version(); return 0; }
    if (opt_help)    { usage(argv[0]); return 0; }

    /* ---- --add ---- */
    if (opt_do_add) {
        if (!opt_add_host || !opt_add_user || !opt_add_pass) {
            fprintf(stderr,"--add requires --host, --username, and --password\n");
            return 1;
        }
        /* Hash the password — never store plaintext */
        char hash[65]="";
        sha256_hex(opt_add_pass, hash);

        /* Derive alias: connect first to get server name, use host as fallback */
        char recv_buf[TOOL_RECV_BUF]; int recv_len=0;
        char srvname[128]="";
        sock_t s = do_connect_auth(opt_add_host, opt_add_port, opt_add_user, hash,
                                   srvname, sizeof(srvname), recv_buf, &recv_len, opt_debug, opt_quiet);
        if (s == INVALID_SOCK) return 1;
        sock_close(s); net_cleanup();

        /* Build alias from server name (spaces → underscores) */
        const char *raw = srvname[0] ? srvname : opt_add_host;
        char alias[64]="";
        strncpy(alias, raw, sizeof(alias)-1);
        int ai; for (ai=0; alias[ai]; ai++) if (alias[ai]==' ') alias[ai]='_';

        if (creds_upsert(alias, opt_add_host, opt_add_port, opt_add_user, hash)) {
            if (creds_write())
                printf("Saved profile '%s' in %s\n", alias, creds_path());
        }
        return 0;
    }

    /* ---- --list ---- */
    if (opt_list) {
        if (!g_nprofiles) {
            printf("No saved profiles. Use --add to add one.\n");
            return 0;
        }
        printf("%-20s %-32s %-6s %s\n","Alias","Host","Port","Nick");
        printf("%-20s %-32s %-6s %s\n","-----","----","----","----");
        int li;
        for (li = 0; li < g_nprofiles; li++) {
            Profile *p = &g_profiles[li];
            printf("%-20s %-32s %-6d %s\n", p->alias, p->host, p->port, p->nick);
        }
        return 0;
    }

    /* ---- --show ---- */
    if (opt_show) {
        Profile *p = creds_find(opt_show);
        if (!p) { fprintf(stderr,"Profile '%s' not found.\n", opt_show); return 1; }
        printf("Alias : %s\n", p->alias);
        printf("Host  : %s\n", p->host);
        printf("Port  : %d\n", p->port);
        printf("Nick  : %s\n", p->nick);
        printf("Hash  : %s\n", p->hash[0] ? p->hash : "(not set)");
        return 0;
    }

    /* ---- --remove ---- */
    if (opt_remove) return cmd_delete(opt_remove);

    /* ---- --rename ---- */
    if (opt_rename_old) return cmd_rename(opt_rename_old, opt_rename_new);

    /* ---- --status ---- */
    if (opt_status) {
        const char *host; int port; char nick[MAX_NICK_LEN]=""; char hash[65]="";
        if (!resolve_profile(opt_status, &host, &port, nick, hash)) return 1;
        char recv_buf[TOOL_RECV_BUF]; int recv_len=0;
        char srvname[128]="";
        if (opt_debug) {
            fprintf(stderr,"[debug] nick : %s\n", nick);
            fprintf(stderr,"[debug] host : %s:%d\n", host, port);
        }
        sock_t s = do_connect_auth(host, port, nick, hash,
                                   srvname, sizeof(srvname), recv_buf, &recv_len, opt_debug, opt_quiet);
        if (s == INVALID_SOCK) return 1;
        printf("OK  server='%s'  host=%s:%d  user=%s\n", srvname, host, port, nick);
        sock_close(s); net_cleanup();
        return 0;
    }

    /* ---- --connect ---- */
    if (opt_connect) {
        const char *host; int port; char nick[MAX_NICK_LEN]=""; char hash[65]="";
        if (!resolve_profile(opt_connect, &host, &port, nick, hash)) return 1;
        char recv_buf[TOOL_RECV_BUF]; int recv_len=0;
        char srvname[128]="";
        if (opt_debug) {
            fprintf(stderr,"[debug] nick : %s\n", nick);
            fprintf(stderr,"[debug] host : %s:%d\n", host, port);
        }
        sock_t s = do_connect_auth(host, port, nick, hash,
                                   srvname, sizeof(srvname), recv_buf, &recv_len, opt_debug, opt_quiet);
        if (s == INVALID_SOCK) return 1;
        run_interactive(s, recv_buf, &recv_len, opt_quiet);
        sock_close(s); net_cleanup();
        return 0;
    }

    /* ---- --command ---- */
    if (opt_cmd_alias) {
        const char *host; int port; char nick[MAX_NICK_LEN]=""; char hash[65]="";
        if (!resolve_profile(opt_cmd_alias, &host, &port, nick, hash)) return 1;
        char recv_buf[TOOL_RECV_BUF]; int recv_len=0;
        char srvname[128]="";
        if (opt_debug) {
            fprintf(stderr,"[debug] nick : %s\n", nick);
            fprintf(stderr,"[debug] host : %s:%d\n", host, port);
        }
        sock_t s = do_connect_auth(host, port, nick, hash,
                                   srvname, sizeof(srvname), recv_buf, &recv_len, opt_debug, opt_quiet);
        if (s == INVALID_SOCK) return 1;
        run_single(s, recv_buf, &recv_len, opt_cmd_str);
        net_send(s, "remoteadmin quit");  /* clean disconnect */
        sock_close(s); net_cleanup();
        return 0;
    }

    /* ---- No action ---- */
    usage(argv[0]);
    return 0;
}
