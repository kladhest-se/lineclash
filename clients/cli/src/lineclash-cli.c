/*
 * lineclash-cli — Lightweight text-mode LineClash client
 *
 * Runs on macOS and Linux. No SDL2, no sound.
 * Uses ANSI escape codes for terminal rendering (requires a VT100-compatible terminal).
 *
 * Compile: see cli/Makefile
 * Usage:   lineclash-cli [host] [port] [nick] [password]
 *          lineclash-cli myserver.example.com 31457 Styrbjorn secret
 *
 * Controls (in game):
 *   Arrow keys    Move / rotate piece
 *   Z             Rotate counter-clockwise
 *   Space         Hard drop
 *   Tab / Enter   Open chat
 *   Q / Ctrl-C    Quit / leave room
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>   /* strcasecmp */
#include <errno.h>
#include "../../shared/include/lineclash.h"

static int g_debug = 0;
static int g_term_raw_active = 0;
static FILE *g_debug_fp = NULL;
#define DLOG(...) do { if(g_debug_fp){ fprintf(g_debug_fp,__VA_ARGS__); fflush(g_debug_fp); } } while(0)

/* ============================================================
 * Embedded SHA-256 + HMAC (copied from client.h)
 * ============================================================ */
typedef unsigned char      _u8;
typedef unsigned int       _u32;
typedef unsigned long long _u64;
typedef struct{_u32 h[8];_u8 buf[64];_u64 total;} _Sha256;
static const _u32 _K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define RR(v,n) (((v)>>(n))|((v)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define S0(x) (RR(x,2)^RR(x,13)^RR(x,22))
#define S1(x) (RR(x,6)^RR(x,11)^RR(x,25))
#define G0(x) (RR(x,7)^RR(x,18)^((x)>>3))
#define G1(x) (RR(x,17)^RR(x,19)^((x)>>10))
static void _sinit(_Sha256*c){
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;
    c->total=0;}
static void _scomp(_Sha256*c,const _u8*b){
    _u32 w[64],a,b2,cc,d,e,f,g,h,t1,t2,i;
    for(i=0;i<16;i++) w[i]=(((_u32)b[i*4])<<24)|(((_u32)b[i*4+1])<<16)|(((_u32)b[i*4+2])<<8)|((_u32)b[i*4+3]);
    for(i=16;i<64;i++) w[i]=G1(w[i-2])+w[i-7]+G0(w[i-15])+w[i-16];
    a=c->h[0];b2=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for(i=0;i<64;i++){t1=h+S1(e)+CH(e,f,g)+_K[i]+w[i];t2=S0(a)+MJ(a,b2,cc);h=g;g=f;f=e;e=d+t1;d=cc;cc=b2;b2=a;a=t1+t2;}
    c->h[0]+=a;c->h[1]+=b2;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;}
static void _supd(_Sha256*c,const _u8*data,size_t len){
    size_t i=c->total%64; c->total+=len;
    while(len--){c->buf[i++]=*data++;if(i==64){_scomp(c,c->buf);i=0;}}}
static void _sfin(_Sha256*c,_u8 d[32]){
    size_t i=c->total%64,j; c->buf[i++]=0x80;
    if(i>56){while(i<64)c->buf[i++]=0;_scomp(c,c->buf);i=0;}
    while(i<56)c->buf[i++]=0;
    _u64 bits=c->total*8;
    for(j=0;j<8;j++) c->buf[56+j]=(_u8)(bits>>((7-j)*8));
    _scomp(c,c->buf);
    for(i=0;i<8;i++){d[i*4]=(c->h[i]>>24)&0xff;d[i*4+1]=(c->h[i]>>16)&0xff;d[i*4+2]=(c->h[i]>>8)&0xff;d[i*4+3]=c->h[i]&0xff;}}

static void sha256_hex(const char *input, char out[65]) {
    _Sha256 ctx; _u8 dig[32]; int i;
    _sinit(&ctx); _supd(&ctx,(const _u8*)input,strlen(input)); _sfin(&ctx,dig);
    for(i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)dig[i]);
    out[64]='\0';
}
static void hmac_sha256_hex(const char pw_hash_hex[65], const char *nonce, char out[65]) {
    _u8 key[32],ipad[64],opad[64],inner[32],final_dig[32]; _Sha256 ctx; int i;
    for(i=0;i<32;i++){unsigned int b=0;sscanf(pw_hash_hex+i*2,"%02x",&b);key[i]=(_u8)b;}
    for(i=0;i<64;i++){_u8 k=(i<32)?key[i]:0;ipad[i]=k^0x36;opad[i]=k^0x5c;}
    _sinit(&ctx);_supd(&ctx,ipad,64);_supd(&ctx,(const _u8*)nonce,strlen(nonce));_sfin(&ctx,inner);
    _sinit(&ctx);_supd(&ctx,opad,64);_supd(&ctx,inner,32);_sfin(&ctx,final_dig);
    for(i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)final_dig[i]);
    out[64]='\0';
}

/* ============================================================
 * Terminal helpers
 * ============================================================ */
/* write() wrapper that silences warn_unused_result on all compilers */
static void term_write(const char *s, int n) {
    int r = (int)write(STDOUT_FILENO, s, (size_t)n); (void)r;
}

static struct termios g_orig_termios;
static int g_raw_mode = 0;

static void term_restore(void) {
    if (g_raw_mode) {
        /* Reset all ANSI attributes, show cursor, clear screen, home cursor.
         * Do this BEFORE restoring termios so output processing is still
         * in raw mode and the escape codes go through cleanly. */
        term_write("\033[0m", 4);   /* reset colours/bold */
        term_write("\033[?25h", 6); /* show cursor */
        term_write("\033[2J", 4);   /* clear screen */
        term_write("\033[H", 3);    /* cursor to top-left */
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode = 0;
    }
}

static void term_raw(void) {
    /* g_orig_termios already captured at main() startup */
    atexit(term_restore);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?25l");  /* hide cursor */
    fflush(stdout);
    g_raw_mode = 1;
}

static void cls(void)       { printf("\033[2J\033[H"); }
static void gotoxy(int x, int y) { printf("\033[%d;%dH", y+1, x+1); }
static void attr_reset(void){ printf("\033[0m"); }
static void attr_bold(void) { printf("\033[1m"); }
static void fg(int r, int g, int b) { printf("\033[38;2;%d;%d;%dm",r,g,b); }
static void bg(int r, int g, int b) { printf("\033[48;2;%d;%d;%dm",r,g,b); }

static int term_cols(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}
static int term_rows(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
        return ws.ws_row;
    return 24;
}

/* Draw a horizontal line of spaces with a colour */
/* Draw a box (single-line chars) */
static void draw_box(int x, int y, int w, int h) {
    gotoxy(x,y); printf("┌"); for(int i=0;i<w-2;i++) printf("─"); printf("┐");
    for(int i=1;i<h-1;i++) { gotoxy(x,y+i); printf("│"); gotoxy(x+w-1,y+i); printf("│"); }
    gotoxy(x,y+h-1); printf("└"); for(int i=0;i<w-2;i++) printf("─"); printf("┘");
}

/* ============================================================
 * Client state
 * ============================================================ */
#define MAX_CHAT     64
#define MAX_CHATLEN  512
#define CLI_CONNECT   0
#define CLI_AUTH      1
#define CLI_LOBBY_SEL 2
#define CLI_IN_LOBBY  3
#define CLI_PLAYING   4
#define CLI_GAMEOVER  5

typedef struct {
    char text[MAX_CHATLEN];
    char nick[MAX_NICK_LEN];
    int  is_server;   /* 1=server/gmsg, 0=player pline */
    int  is_speedup;  /* 1=speed-up notification, render in bright yellow */
} ChatLine;

typedef struct {
    int  id;
    int  players, max_players;
    int  in_game, paused;
    int  no_attacks, last_player_wins, no_taunts, speedup_enabled;
    int  garbage_system;    /* 0=classic, 1=guideline */
    int  garbage_targeting; /* 0=all, 1=random, 2=focused */
    int  hold_enabled;      /* 1=hold piece allowed */
} RoomInfo;

typedef struct {
    /* Network */
    sock_t  sock;
    char    recv_buf[RECV_BUF_SIZE];
    int     recv_len;

    /* Credentials */
    char    host[128];
    char    server_name[64]; /* received from server during auth */
    int     port;
    char    nick[MAX_NICK_LEN];
    char    pass_hash[65];  /* sha256(password) */
    char    team[MAX_NICK_LEN];

    /* Auth state */
    int     state;
    int     save_on_auth;  /* 1 = save profile after successful auth using server_name */
    char    nonce[65];
    int     is_new_account;
    int     priv_level;     /* 1=admin, 2=mgr, 3=user */

    /* Room list */
    RoomInfo rooms[MAX_LOBBIES];
    int      room_count;
    int      room_cursor;

    /* In-room */
    int      room_id;
    int      slot;
    int      is_host;
    int      is_spectator;
    int      is_ready;
    int      no_attacks, last_player_wins, no_taunts, speedup_enabled;
    int      garbage_system;    /* 0=classic, 1=guideline */
    int      garbage_targeting; /* 0=all, 1=random, 2=focused */
    int      hold_enabled;      /* 1=hold piece allowed */

    /* Players in room */
    char     opp_nicks[MAX_PLAYERS][MAX_NICK_LEN];
    char     opp_teams[MAX_PLAYERS][MAX_NICK_LEN];
    int      opp_alive[MAX_PLAYERS];
    int      opp_ready[MAX_PLAYERS];
    int      opp_level[MAX_PLAYERS];
    int      host_slot;

    /* Game field (own) */
    Field    field;
    Piece    cur, next;
    int      drop_timer, drop_interval;
    int      paused;          /* 1 = game is paused by server */
    /* Hold piece */
    int      hold_type;   /* piece type in hold, -1=empty */
    int      hold_used;   /* 1=hold already used this piece */
    /* Lock delay */
    int      lock_delay_ms;
    int      lock_timer;  /* ms remaining */
    int      lock_active; /* 1=piece grounded, delay running */
    /* Server-supplied piece queue */
    int      srv_pieces[128];
    int      srv_piece_head;
    int      srv_piece_tail;
    /* Bot tracking */
    int      opp_is_bot[MAX_PLAYERS];
    int      opp_bot_diff[MAX_PLAYERS];
    int      bots_allowed;
    int      alive;
    int      attack_target;  /* 1-based slot */
    int      spec_watch_slot; /* 0=none, else slot# of player being watched */
    int      team_input_mode; /* 1=typing team name */
    uint8_t  specials[64];
    int      spec_head, spec_tail;
    /* Opponent fields */
    Field    opp_fields[MAX_PLAYERS];
    int      opp_lines[MAX_PLAYERS];

    /* Leaderboard */
    char     lb_nicks[MAX_PLAYERS][MAX_NICK_LEN];
    int      lb_wins[MAX_PLAYERS];
    int      lb_streaks[MAX_PLAYERS];
    int      lb_count;
    int      round_num;
    /* Team leaderboard */
    char     tlb_teams[MAX_PLAYERS][MAX_NICK_LEN];
    int      tlb_wins[MAX_PLAYERS];
    int      tlb_count;
    char     winner[MAX_NICK_LEN];

    /* Chat */
    ChatLine chat[MAX_CHAT];
    int      chat_count;
    char     chat_input[MAX_CHATLEN];
    int      chat_mode;  /* 0=normal, 1=typing chat */

    /* Global chat */
    ChatLine gchat[MAX_CHAT];
    int      gchat_count;

    /* Redraw flag */
    int      dirty;
    int      running;
} CliCtx;

/* ============================================================
 * Field block → ASCII
 * ============================================================ */
/* ============================================================
 * Chat helpers
 * ============================================================ */
static void add_chat(CliCtx *c, const char *nick, const char *text, int server) {
    if (c->chat_count >= MAX_CHAT) {
        memmove(c->chat, c->chat+1, sizeof(ChatLine)*(MAX_CHAT-1));
        c->chat_count = MAX_CHAT-1;
    }
    ChatLine *l = &c->chat[c->chat_count++];
    strncpy(l->nick, nick, MAX_NICK_LEN-1);
    strncpy(l->text, text, MAX_CHATLEN-1);
    l->is_server = server;
}

static void add_gchat(CliCtx *c, const char *nick, const char *text, int server) {
    if (c->gchat_count >= MAX_CHAT) {
        memmove(c->gchat, c->gchat+1, sizeof(ChatLine)*(MAX_CHAT-1));
        c->gchat_count = MAX_CHAT-1;
    }
    ChatLine *l = &c->gchat[c->gchat_count++];
    strncpy(l->nick, nick, MAX_NICK_LEN-1);
    strncpy(l->text, text, MAX_CHATLEN-1);
    l->is_server = server;
}


/* ============================================================
 * UTF-8 → Latin-1 and emoji/smiley substitution
 * (Copied from SDL2 client for protocol compatibility)
 * ============================================================ */
static void latin1_to_utf8(const char *in, char *out, int outsz) {
    int i=0, o=0;
    while(in[i] && o<outsz-2) {
        unsigned char c=(unsigned char)in[i++];
        if(c<0x80) { out[o++]=(char)c; }
        else if(c<0xC0) { out[o++]=0xC2; out[o++]=(char)c; }
        else             { out[o++]=0xC3; out[o++]=(char)(c-0x40); }
    }
    out[o]='\0';
}

static void utf8_to_latin1(const char *in, char *out, int outsz) {
    int i=0, o=0;
    while(in[i] && o<outsz-1) {
        unsigned char c=(unsigned char)in[i];
        if(c<0x80)        { out[o++]=in[i++]; }
        else if(c==0xC3 && (unsigned char)in[i+1]>=0x80 && (unsigned char)in[i+1]<=0xBF)
                           { out[o++]=(char)(0xC0|((unsigned char)in[i+1]-0x80)); i+=2; }
        else if(c==0xC2 && (unsigned char)in[i+1]>=0x80 && (unsigned char)in[i+1]<=0xBF)
                           { out[o++]=(char)(0x80|((unsigned char)in[i+1]-0x80)); i+=2; }
        else               { out[o++]=in[i++]; }
    }
    out[o]='\0';
}

static void emoji_sub(const char *in, char *out, int outsz) {
    static const struct { const char *from; const char *to; } map[] = {
        {":)",  "(^_^)"}, {":D",  "(^O^)"}, {":-)", "(^_^)"}, {":-D","(^O^)"},
        {":(", "(;_;)"}, {":-(", "(;_;)"}, {":P",  "(^~^)"}, {":-P","(^~^)"},
        {";)",  "(^-^)"}, {";-)", "(^-^)"}, {":O",  "(O_O)"}, {":-O","(O_O)"},
        {":>",  "(>^<)"}, {"<3",  "(<3)" }, {"B)",  "(B-)"}, {":*",  "(*^*)"},
        {NULL, NULL}
    };
    int i=0, o=0;
    while(in[i]&&o<outsz-1){
        int matched=0, k;
        for(k=0;map[k].from;k++){
            int fl=(int)strlen(map[k].from);
            if(strncmp(in+i,map[k].from,fl)==0){
                int tl=(int)strlen(map[k].to);
                if(o+tl<outsz-1){ memcpy(out+o,map[k].to,tl); o+=tl; }
                i+=fl; matched=1; break;
            }
        }
        if(!matched) out[o++]=in[i++];
    }
    out[o]='\0';
}

/* Apply both conversions to a chat line before display or sending */
static void chat_process(const char *in, char *out, int outsz) {
    char tmp[MAX_CHATLEN];
    utf8_to_latin1(in, tmp, sizeof(tmp));
    emoji_sub(tmp, out, outsz);
}

/* Remove last UTF-8 character from a string (handles multi-byte) */
static void str_backspace_utf8(char *s) {
    int l = (int)strlen(s);
    if (l == 0) return;
    /* Walk back past any UTF-8 continuation bytes (0x80..0xBF) */
    l--;
    while (l > 0 && ((unsigned char)s[l] & 0xC0) == 0x80) l--;
    s[l] = '\0';
}

/* Forward declarations */
static void send_msg(CliCtx *c, const char *fmt, ...);
static void add_chat(CliCtx *c, const char *nick, const char *text, int server);
/* Profile forward declarations (defined later in file) */
#define CLI_PROFILES_MAX 32
typedef struct { char alias[64]; char host[256]; int port;
                 char nick[MAX_NICK_LEN]; char hash[65]; } CliProfile;
static CliProfile g_profiles[CLI_PROFILES_MAX];
static int        g_nprofiles = 0;
static char       g_cfg_path_override[512] = "";
static CliProfile *cli_profile_find(const char *alias);
static int         cli_profiles_save(void);

/* ---- Game mechanics helpers ---- */
static int level_to_interval(int lv) {
    /* NES-style drop intervals in 60fps ticks */
    static const int tbl[] = {48,43,38,33,28,23,18,13,8,6,5,5,5,4,4,4,3,3,3,2,2,2,2,2,2,2,2,2,2,1};
    if (lv < 1)  lv = 1;
    if (lv > 30) lv = 30;
    return tbl[lv - 1];
}

static void lock_and_spawn(CliCtx *c) {
    int freq[NUM_PIECES] = {14,14,14,14,14,14,14};
    field_lock(&c->field, &c->cur);
    int lines = field_clear_lines(&c->field);
    c->field.lines += lines;
    c->field.score += lines * lines * 100 * c->field.level;
    if (lines > 0) {
        /* Tell server lines cleared so it awards specials */
        { char lcbuf[32]; snprintf(lcbuf,sizeof(lcbuf),"lineclear %d",lines);
          send_msg(c, "%s", lcbuf); }
        /* Level via fieldack, specials via addspecial */
        /* Send garbage to opponents (2 lines→1 garbage, 3→2, 4→4) */
        /* Garbage is server-authoritative — handled on lineclear command */
        send_msg(c, "lvl %d %d", c->slot, c->field.level);
    }
    proto_send_field(c->sock, c->slot, &c->field);
    /* Next piece — use server-supplied queue */
    c->hold_used = 0; /* hold resets each new piece */
    c->lock_active = 0; c->lock_timer = 0;
    c->cur = c->next;
    if (c->srv_piece_head < c->srv_piece_tail)
        piece_init(&c->next, c->srv_pieces[c->srv_piece_head++ % 128]);
    else
        piece_random(&c->next, freq);
    if (!piece_spawn(&c->field, &c->cur)) {
        /* Topped out */
        /* Send playerlost first — server marks us dead before processing field */
        send_msg(c, "playerlost %d", c->slot);
        proto_send_field(c->sock, c->slot, &c->field);
        c->alive = 0;
        c->state = CLI_GAMEOVER;
        add_chat(c, "***", "You topped out!", 1);
    }
    c->drop_timer = c->drop_interval;
    c->dirty = 1;
}

/* ============================================================
 * Draw functions
 * ============================================================ */
#define W_COLS term_cols()
#define W_ROWS term_rows()

static void draw_header(CliCtx *c) {
    int cols = term_cols();
    gotoxy(0,0); bg(20,20,46); fg(180,180,255); attr_bold();
    printf("%-*s", cols, " LineClash");
    attr_reset();
    /* Status right side */
    char st[256];
    if (c->state == CLI_LOBBY_SEL)
        snprintf(st, sizeof(st), " [LOBBY] %s ", c->nick);
    else if (c->state == CLI_IN_LOBBY || c->state == CLI_PLAYING || c->state == CLI_GAMEOVER)
        snprintf(st, sizeof(st), " Room %d | %s ", c->room_id, c->nick);
    else
        snprintf(st, sizeof(st), " %s:%d ", c->host, c->port);
    gotoxy(cols-(int)strlen(st)-1, 0);
    bg(30,30,70); fg(150,150,255);
    printf("%s", st);
    attr_reset();
}

static void draw_connect_screen(CliCtx *c) {
    int cols = term_cols(), rows = term_rows();
    cls();
    draw_header(c);
    int cx = cols/2 - 20, cy = rows/2 - 4;
    draw_box(cx-2, cy-1, 44, 10);
    gotoxy(cx+12, cy-1); fg(100,180,255); attr_bold(); printf("CONNECT"); attr_reset();
    gotoxy(cx, cy+1); fg(150,150,150); printf("Host    : "); fg(255,255,255); printf("%-28s", c->host);
    gotoxy(cx, cy+2); fg(150,150,150); printf("Port    : "); fg(255,255,255); printf("%-28d", c->port);
    gotoxy(cx, cy+3); fg(150,150,150); printf("Nick    : "); fg(255,255,255); printf("%-28s", c->nick);
    gotoxy(cx, cy+4); fg(150,150,150); printf("Password: "); fg(255,255,255); printf("%-28s", "***");
    gotoxy(cx, cy+6); fg(100,200,100); printf("Connecting...");
    attr_reset();
    /* PAUSED overlay */
    if (c->paused) {
        int pc = cols/2 - 5, pr = rows/2;
        fg(255,220,50); attr_bold();
        gotoxy(pc-2, pr-1); printf("╔══════════╗");
        gotoxy(pc-2, pr);   printf("║  PAUSED  ║");
        gotoxy(pc-2, pr+1); printf("╚══════════╝");
        attr_reset();
    }
    fflush(stdout);
}

static void draw_lobby_select(CliCtx *c) {
    int cols = term_cols(), rows = term_rows();
    cls();
    draw_header(c);
    /* Header width: 8+1+6+1+8+1+10+1+12+1+10+1+10 = 70 chars */
    const int hdr_w = 70;
    int lx = (cols - hdr_w) / 2, ly = 3;
    /* Column headers */
    gotoxy(lx,ly); fg(100,100,180); attr_bold();
    printf("%-8s %-6s %-8s %-10s %-12s %-10s %-10s %-10s", "Room","State","Players","Specials","Last-Wins","Taunts","Speed-Up","Garbage");
    attr_reset();
    ly++;
    gotoxy(lx,ly); fg(60,60,100);
    for(int i=0;i<hdr_w;i++) printf("─");
    ly++;
    for (int i=0; i<c->room_count; i++) {
        RoomInfo *r = &c->rooms[i];
        int cur = (i == c->room_cursor);
        int r_full = (!r->in_game && r->max_players > 0 && r->players >= r->max_players);
        const char *st = r->in_game ? (r->paused ? "PAUSED" : "PLAYING")
                       : r_full ? "FULL" : "WAITING";
        gotoxy(lx,ly+i);
        if (cur && r_full)  { bg(40,10,10); fg(255,140,140); attr_bold(); }
        else if (cur)       { bg(30,40,80); fg(200,220,255); attr_bold(); }
        else if (r_full)    { attr_reset(); fg(120, 60, 60); }
        else if (r->in_game){ attr_reset(); fg(180, 140, 140); }
        else                { attr_reset(); fg(200, 200, 200); }
        printf(" %-7d %-6s %d/%-6d %-10s %-12s %-10s %-10s %-10s",
            r->id, st, r->players, r->max_players,
            r->no_attacks       ? "OFF" : "ON",
            r->last_player_wins ? "ON"  : "OFF",
            r->no_taunts        ? "OFF" : "ON",
            r->speedup_enabled  ? "ON"  : "OFF",
            r->garbage_system   ? "GUIDELINE" : "CLASSIC");
        if (cur) { printf("  ←"); }
        attr_reset();
    }
    /* Key hints */
    int hy = ly + c->room_count + 2;
    gotoxy(lx, hy); fg(80,80,120);
    printf("↑↓=Navigate  Enter=Join  Q=Quit");

    /* Global chat panel */
    int chatx = lx, chaty = hy + 2;
    int chath = rows - chaty - 2;
    if (chath > 3) {
        gotoxy(chatx, chaty); fg(60,120,60); attr_bold(); printf("─── SERVER CHAT ──────────────────────────────────────");
        attr_reset();
        int vis = chath - 2;
        int start = c->gchat_count > vis ? c->gchat_count - vis : 0;
        for (int i=start; i<c->gchat_count && i<start+vis; i++) {
            gotoxy(chatx, chaty+1+(i-start));
            char u8gl[MAX_CHATLEN*2];
            latin1_to_utf8(c->gchat[i].text, u8gl, sizeof(u8gl));
            if (c->gchat[i].is_speedup) { fg(255,220,0); attr_bold(); printf("%s", u8gl); attr_reset(); }
            else if (c->gchat[i].is_server) { fg(120,180,120); printf("%s", u8gl); }
            else { fg(100,150,100); printf("<%s> ", c->gchat[i].nick);
                   fg(180,220,180); printf("%s", u8gl); }
            attr_reset();
        }
        /* Input */
        gotoxy(chatx, chaty+chath-1);
        fg(60,120,60); printf("> "); fg(200,255,200);
        if (c->chat_mode) printf("%s█", c->chat_input);
        else { fg(60,80,60); printf("Tab to chat..."); }
        attr_reset();
    }
    attr_reset(); fflush(stdout);
}

static void draw_in_room(CliCtx *c) {
    int cols = term_cols(), rows = term_rows();
    cls();
    draw_header(c);

    int lx = 1, ly = 2;

    /* Room info bar */
    gotoxy(lx, ly); fg(180,180,80); attr_bold();
    printf("Room %d", c->room_id);
    attr_reset(); fg(120,120,80);
    printf("  specials:%s  last-wins:%s  taunts:%s  speedup:%s  garbage:%s  target:%s",
        c->no_attacks ? "OFF" : "ON",
        c->last_player_wins ? "ON" : "OFF",
        c->no_taunts ? "OFF" : "ON",
        c->speedup_enabled ? "ON" : "OFF",
        c->garbage_system ? "guideline" : "classic",
        c->garbage_targeting==0?"all":c->garbage_targeting==1?"random":"focused");
    ly += 2;

    /* Player roster */
    gotoxy(lx, ly); fg(100,180,255); attr_bold(); printf("Players:"); attr_reset(); ly++;
    /* Self */
    {
        const char *rdy = c->is_ready ? "[RDY]" : "[   ]";
        gotoxy(lx, ly);
        fg(c->is_ready ? 100 : 150, 200, c->is_ready ? 100 : 80);
        printf("  %s %s", rdy, c->nick);
        if (c->is_host) { fg(255,200,50); printf(" [HOST]"); }
        attr_reset(); ly++;
    }
    for (int i=0; i<MAX_PLAYERS; i++) {
        if (!c->opp_nicks[i][0]) continue;
        gotoxy(lx, ly);
        const char *rdy = c->opp_ready[i] ? "[RDY]" : "[   ]";
        fg(c->opp_ready[i] ? 100 : 150, 180, c->opp_ready[i] ? 100 : 100);
        if (c->opp_is_bot[i]) {
            const char *diff_tag = c->opp_bot_diff[i]==2?"[HARD]":
                                   c->opp_bot_diff[i]==1?"[MED] ":"[EASY]";
            printf("  %s %s %s [BOT]", rdy, diff_tag, c->opp_nicks[i]);
        } else {
            printf("  %s %s", rdy, c->opp_nicks[i]);
        }
        if ((i+1) == c->host_slot) { fg(255,200,50); printf(" [HOST]"); }
        attr_reset(); ly++;
    }
    ly++;

    /* Player + team wins */
    if (c->lb_count > 0) {
        gotoxy(lx, ly); fg(255,200,50); attr_bold();
        printf("PLAYER WINS:"); attr_reset(); ly++;
        for (int i=0; i<c->lb_count && i<6; i++) {
            gotoxy(lx, ly++);
            fg(i==0?255:180, i==0?200:180, 50);
            if (c->lb_streaks[i]>=2) { fg(255,140,0); printf("  %d. %.10s %d 🔥%d", i+1, c->lb_nicks[i], c->lb_wins[i], c->lb_streaks[i]); }
            else printf("  %d. %.12s  %d", i+1, c->lb_nicks[i], c->lb_wins[i]);
            attr_reset();
        }
        ly++;
    }
    if (c->tlb_count > 0) {
        gotoxy(lx, ly); fg(100,200,255); attr_bold();
        printf("TEAM WINS:"); attr_reset(); ly++;
        for (int i=0; i<c->tlb_count && i<4; i++) {
            gotoxy(lx, ly++);
            fg(i==0?100:80, i==0?220:180, i==0?255:200);
            printf("  %d. %.12s  %d", i+1, c->tlb_teams[i], c->tlb_wins[i]);
            attr_reset();
        }
        ly++;
    }

    /* Options */
    gotoxy(lx, ly); fg(100,100,180); attr_bold(); printf("OPTIONS:"); attr_reset(); ly++;
    gotoxy(lx, ly);
    fg(100,200,100); printf("  [%s] Specials  [%s] Last-Wins  [%s] Taunts  [%s] Speed-Up  [%s] Garbage  [%s] Target",
        c->no_attacks ? "OFF" : "ON ",
        c->last_player_wins ? "ON " : "OFF",
        c->no_taunts ? "OFF" : "ON ",
        c->speedup_enabled ? "ON " : "OFF",
        c->garbage_system ? "GDL" : "CLS",
        c->garbage_targeting==0?"ALL":c->garbage_targeting==1?"RND":"FOC");
    attr_reset(); ly += 2;

    /* Buttons */
    if (c->state == CLI_IN_LOBBY || c->state == CLI_GAMEOVER) {
        if (c->state == CLI_GAMEOVER) {
            gotoxy(lx, ly); fg(160,60,60); attr_bold();
            printf("Eliminated — waiting for round to end...");
            attr_reset();
        } else {
        gotoxy(lx, ly); fg(100,200,100); attr_bold();
        printf(c->is_ready ? "[R] Unready" : "[R] Ready");
        attr_reset();
        }
        if (c->is_host && c->state==CLI_IN_LOBBY) {
            fg(255,200,50); printf("   [S] Start"); attr_reset();
            if (c->bots_allowed) {
                fg(100,200,100);
                printf("  Bot: [E]asy [M]edium [H]ard  [X] Remove");
                attr_reset();
            }
        } else if (c->is_host) {
            fg(255,200,50); printf("   [S] Start"); attr_reset();
        }
        if ((c->is_host || c->priv_level <= 2) && c->state == CLI_IN_LOBBY) {
            gotoxy(lx, ly); ly++;
            fg(80,80,140); printf("Options: [1] Specials  [2] Last-Wins  [3] Taunts  [4] Speed-Up  [5] Garbage  [6] Target");
            attr_reset();
        }
        ly++;
    }

    /* Winner */
    if (c->winner[0] && c->state == CLI_GAMEOVER) {
        ly++;
        gotoxy(lx, ly); fg(255,215,0); attr_bold();
        printf("★ Winner: %s ★", c->winner);
        attr_reset(); ly++;
    }

    /* Two chat windows at the bottom, split horizontally.
     * Left half: ROOM CHAT (pline) with input.
     * Right half: ACTIVITY (gmsg/server events, no input). */
    {
        int chat_top = rows - 11;
        if (chat_top < ly + 1) chat_top = ly + 1;
        int chat_rows = rows - chat_top - 4; /* header + blank + input + keyhints */
        if (chat_rows < 1) chat_rows = 1;
        int half = cols / 2;

        /* ─── LEFT: ROOM CHAT ─── */
        gotoxy(0, chat_top); fg(60,60,180); attr_bold();
        printf("── ROOM CHAT "); for(int i=12;i<half-1;i++) printf("─");
        attr_reset();
        /* Vertical divider */
        for (int row=chat_top; row<rows-1; row++) {
            gotoxy(half, row); fg(40,40,80); printf("│"); attr_reset();
        }
        /* Room chat messages (player plines + non-system gmsg) */
        int rvis = chat_rows;
        int rstart = c->chat_count > rvis ? c->chat_count - rvis : 0;
        for (int i=rstart; i<c->chat_count && i<rstart+rvis; i++) {
            gotoxy(0, chat_top+1+(i-rstart));
            char u8r[MAX_CHATLEN*2];
            latin1_to_utf8(c->chat[i].text, u8r, sizeof(u8r));
            if (c->chat[i].is_server) {
                fg(120,120,200); printf("%-*.*s", half-1, half-1, u8r);
            } else {
                fg(100,100,180);
                int nlen=(int)strlen(c->chat[i].nick)+3;
                printf("<%s> ", c->chat[i].nick);
                fg(180,180,220);
                printf("%-*.*s", half-1-nlen, half-1-nlen, u8r);
            }
            attr_reset();
        }
        /* Input line */
        gotoxy(0, rows-2); fg(60,60,180); printf("> "); fg(200,200,255);
        if (c->chat_mode) printf("%s█", c->chat_input);
        else { fg(50,50,100); printf("Tab to chat..."); }
        attr_reset();

        /* ─── RIGHT: ACTIVITY LOG ─── */
        gotoxy(half+1, chat_top); fg(80,120,80); attr_bold();
        printf("── ACTIVITY "); for(int i=12;i<cols-half-2;i++) printf("─");
        attr_reset();
        /* Show gmsg/server events from gchat (game events, specials, kills) */
        int avis = chat_rows;
        int astart = c->gchat_count > avis ? c->gchat_count - avis : 0;
        int actw = cols - half - 2;
        for (int i=astart; i<c->gchat_count && i<astart+avis; i++) {
            gotoxy(half+1, chat_top+1+(i-astart));
            char u8a[MAX_CHATLEN*2];
            latin1_to_utf8(c->gchat[i].text, u8a, sizeof(u8a));
            if (c->gchat[i].is_speedup) {
                fg(255,220,0); attr_bold(); printf("%-*.*s", actw, actw, u8a); attr_reset();
            } else if (c->gchat[i].is_server) {
                fg(120,180,120); printf("%-*.*s", actw, actw, u8a);
            } else {
                fg(100,150,100);
                int nlen2=(int)strlen(c->gchat[i].nick)+3;
                printf("<%s> ", c->gchat[i].nick);
                fg(180,220,180);
                printf("%-*.*s", actw-nlen2, actw-nlen2, u8a);
            }
            attr_reset();
        }
    }

    /* Key hints at bottom */
    gotoxy(0, rows-1); fg(60,60,100);
    if (c->team_input_mode) {
        fg(180,180,60); printf("Team name (Enter=Set  Esc=Cancel): ");
        fg(255,255,120); printf("%s█", c->chat_input);
    } else if (c->state == CLI_PLAYING && !c->is_spectator) {
        printf("← →=Move  ↑=Rot  ↓=Drop  Spc=Hard  C=Hold  1-8=Target  Tab=Chat  Q=Leave");
    } else if (c->state == CLI_PLAYING && c->is_spectator) {
        printf("←/→=Watch prev/next player  Tab=Chat  Q=Leave");
    } else if (c->state == CLI_GAMEOVER) {
        printf("←/→=Watch players  Tab=Chat  Q=Leave room");
    } else {
        printf("R=Ready  S=Start(host)  T=Set team  1-4=Toggle options  Tab=Chat  Q=Leave");
    }
    attr_reset();
    fflush(stdout);
}

static void draw_playing(CliCtx *c) {
    int cols = term_cols(), rows = term_rows();
    cls();
    draw_header(c);

    /* ── Field layout ──────────────────────────────────────────────────────
     * Row 1 (fy=2): own-field header "[slot] nick  Lv:N  Lines:N  Score:N"
     * Rows fy+1 .. fy+FIELD_HEIGHT+1: field + borders
     * To the right of the field: NEXT piece box + SPECIALS + WINS
     * Opponents: right of the sidebar, each FIELD_WIDTH+3 chars wide, half-height
     * Bottom: ROOM CHAT (left half) | ACTIVITY (right half), then key hints
     */

    int fx = 1, fy = 2;

    /* ── Own-field header (slot + nick + live stats) ─────────────────────── */
    /* When dead/spectator: optionally show a watched opponent's field */
    int watch_idx = -1;
    if ((!c->alive || c->is_spectator) && c->spec_watch_slot > 0) {
        int wi = c->spec_watch_slot - 1;
        if (wi >= 0 && wi < MAX_PLAYERS && c->opp_nicks[wi][0])
            watch_idx = wi;
    }
    gotoxy(fx, fy-1);
    if (watch_idx >= 0) {
        fg(255,220,80); attr_bold();
        printf("WATCHING [%d] %.14s", watch_idx+1, c->opp_nicks[watch_idx]);
        fg(140,180,140);
        printf("  Lv:%-3d Lines:%-4d",
               c->opp_fields[watch_idx].level, c->opp_lines[watch_idx]);
        attr_reset();
        fg(80,80,120); printf("  ← → to cycle");
    } else {
        fg(180,180,255); attr_bold();
        printf("[%d] %.14s", c->slot, c->nick);
        attr_reset();
        fg(140,140,180);
        printf("  Lv:"); fg(255,255,200); printf("%-3d", c->field.level);
        fg(140,140,180);
        printf(" Lines:"); fg(255,255,200); printf("%-4d", c->field.lines);
        fg(140,140,180);
        printf(" Score:"); fg(255,255,200); printf("%d", c->field.score);
    }
    attr_reset();

    /* ── Build display copy with ghost + live piece ───────────────────────── */
    Field disp = (watch_idx >= 0) ? c->opp_fields[watch_idx] : c->field;
    if (watch_idx < 0 && c->alive && c->cur.type >= 0 && c->cur.type < NUM_PIECES) {
        Piece ghost = c->cur;
        piece_drop(&c->field, &ghost);
        if (ghost.y > c->cur.y) {
            int r = ghost.rotation & 3;
            for (int pi=0; pi<4; pi++) {
                int bx = ghost.x + PIECES[ghost.type][r].c[pi].dx;
                int by2 = ghost.y + PIECES[ghost.type][r].c[pi].dy;
                if (by2>=0 && by2<FIELD_HEIGHT && bx>=0 && bx<FIELD_WIDTH && disp.cells[by2][bx]==0)
                    disp.cells[by2][bx] = (uint8_t)(128 + c->cur.type);
            }
        }
    }
    if (c->alive && c->cur.type >= 0 && c->cur.type < NUM_PIECES) {
        int r = c->cur.rotation & 3;
        for (int pi=0; pi<4; pi++) {
            int bx = c->cur.x + PIECES[c->cur.type][r].c[pi].dx;
            int by2 = c->cur.y + PIECES[c->cur.type][r].c[pi].dy;
            if (by2>=0 && by2<FIELD_HEIGHT && bx>=0 && bx<FIELD_WIDTH)
                disp.cells[by2][bx] = (uint8_t)(c->cur.type + 1);
        }
    }

    /* ── Piece colours ───────────────────────────────────────────────────── */
    static const int PCOL[8][3] = {
        {0,200,255},{255,220,0},{180,0,255},{80,255,80},
        {255,60,60},{60,100,255},{255,140,0},{200,200,200}};

    /* ── Draw own field ──────────────────────────────────────────────────── */
    gotoxy(fx, fy); fg(80,80,120);
    printf("┌"); for(int i=0;i<FIELD_WIDTH*2;i++) printf("─"); printf("┐");
    for (int row=0; row<FIELD_HEIGHT; row++) {
        gotoxy(fx, fy+1+row); fg(80,80,120); printf("│");
        for (int col=0; col<FIELD_WIDTH; col++) {
            uint8_t v = disp.cells[row][col];
            if (v == 0) { bg(18,18,32); printf("  "); }
            else if (v >= 128) {
                int *pc=(int*)PCOL[(v-128)%8];
                bg(pc[0]/6,pc[1]/6,pc[2]/6); fg(pc[0]/2,pc[1]/2,pc[2]/2);
                printf("▒▒");
            } else if (v >= 8) { bg(200,140,0); fg(255,220,80); printf("▪▪"); }
            else {
                int *pc=(int*)PCOL[(v-1)%8];
                bg(pc[0]*2/3,pc[1]*2/3,pc[2]*2/3); fg(pc[0],pc[1],pc[2]);
                printf("██");
            }
        }
        attr_reset(); fg(80,80,120); printf("│");
    }
    gotoxy(fx, fy+1+FIELD_HEIGHT); fg(80,80,120);
    printf("└"); for(int i=0;i<FIELD_WIDTH*2;i++) printf("─"); printf("┘");
    attr_reset();

    /* ── Sidebar (right of own field) ────────────────────────────────────── */
    int sx = fx + FIELD_WIDTH*2 + 3, sy = fy;

    /* HOLD piece box */
    gotoxy(sx, sy); fg(150,150,200); attr_bold(); printf("HOLD"); attr_reset();
    fg(80,80,120); printf(" [C]"); attr_reset();
    if (c->hold_used) { fg(80,80,100); printf(" (used)"); } attr_reset();
    for (int row=0; row<4; row++) {
        gotoxy(sx, sy+1+row);
        for (int col=0; col<4; col++) { bg(12,12,24); printf("  "); }
        attr_reset();
    }
    if (c->hold_type >= 0 && c->hold_type < NUM_PIECES) {
        int r=0, *hpc=(int*)PCOL[c->hold_type%8];
        for (int pi=0;pi<4;pi++) {
            int dx=PIECES[c->hold_type][r].c[pi].dx;
            int dy=PIECES[c->hold_type][r].c[pi].dy;
            int nx=dx+1, ny=dy+1;
            if (nx>=0&&nx<4&&ny>=0&&ny<4) {
                gotoxy(sx+nx*2, sy+1+ny);
                if (c->hold_used) { bg(hpc[0]/4,hpc[1]/4,hpc[2]/4); fg(hpc[0]/2,hpc[1]/2,hpc[2]/2); }
                else              { bg(hpc[0]*2/3,hpc[1]*2/3,hpc[2]*2/3); fg(hpc[0],hpc[1],hpc[2]); }
                printf("██"); attr_reset();
            }
        }
    }

    /* NEXT piece box */
    int ny_off = 6; /* shift NEXT down below HOLD */
    gotoxy(sx, sy+ny_off); fg(150,150,200); attr_bold(); printf("NEXT"); attr_reset();
    /* 4×4 mini grid */
    for (int row=0; row<4; row++) {
        gotoxy(sx, sy+ny_off+1+row);
        for (int col=0; col<4; col++) { bg(18,18,32); printf("  "); }\
        attr_reset();
    }
    if (c->next.type >= 0 && c->next.type < NUM_PIECES) {
        /* Center piece in 4×4 box using only dx/dy offsets */
        int r = 0; /* always show next at rotation 0 */
        int *pc = (int*)PCOL[c->next.type % 8];
        for (int pi=0; pi<4; pi++) {
            int dx = PIECES[c->next.type][r].c[pi].dx;
            int dy = PIECES[c->next.type][r].c[pi].dy;
            int nx = dx + 1; /* shift right by 1 to center */
            int ny = dy + 1; /* shift down by 1 to center */
            if (nx>=0&&nx<4&&ny>=0&&ny<4) {
                gotoxy(sx + nx*2, sy+ny_off+1+ny);
                bg(pc[0]*2/3,pc[1]*2/3,pc[2]*2/3); fg(pc[0],pc[1],pc[2]);
                printf("██"); attr_reset();
            }
        }
    }

    /* SPECIALS */
    int ssy = sy + 12; /* below HOLD(6 rows) + NEXT(6 rows) */
    gotoxy(sx, ssy); fg(150,150,200); attr_bold(); printf("SPECIALS:"); attr_reset();
    gotoxy(sx, ssy+1);
    int sp_shown=0, sp_h=c->field.special_head, sp_t=c->field.special_tail;
    for(int si=sp_h; si!=sp_t && sp_shown<8; si=(si+1)%64, sp_shown++) {
        uint8_t sv=c->field.specials[si];
        char sc=(sv>=1&&sv<=NUM_SPECIALS)?SPECIAL_CHARS[sv-1]:'?';
        fg(255,180,50); printf("%c ", sc);
    }
    if(sp_shown==0) { fg(80,80,100); printf("(empty)"); }
    attr_reset();

    /* PLAYER WINS */
    int wy = ssy + 3;
    if (c->lb_count > 0) {
        gotoxy(sx, wy); fg(255,200,50); attr_bold(); printf("PLAYER WINS:"); attr_reset(); wy++;
        for(int i=0; i<c->lb_count && i<4; i++) {
            gotoxy(sx, wy++);
            fg(i==0?255:180, i==0?200:180, 50);
            if (c->lb_streaks[i]>=2) { fg(255,140,0); printf("  %d. %.8s %d x%d!", i+1, c->lb_nicks[i], c->lb_wins[i], c->lb_streaks[i]); attr_reset(); }
            else printf("  %d. %.10s %d", i+1, c->lb_nicks[i], c->lb_wins[i]);
            attr_reset();
        }
        wy++;
    }
    /* TEAM WINS */
    if (c->tlb_count > 0) {
        gotoxy(sx, wy); fg(100,200,255); attr_bold(); printf("TEAM WINS:"); attr_reset(); wy++;
        for(int i=0; i<c->tlb_count && i<4; i++) {
            gotoxy(sx, wy++);
            fg(i==0?100:80, i==0?220:180, i==0?255:200);
            printf("  %d. %.10s %d", i+1, c->tlb_teams[i], c->tlb_wins[i]);
            attr_reset();
        }
    }

    /* Winner overlay */
    if (c->winner[0] && c->state == CLI_GAMEOVER) {
        int wr=rows/2, wc=cols/2-15;
        gotoxy(wc,wr-1); bg(50,50,0); fg(255,215,0); attr_bold();
        printf("  ┌───────────────────────────┐  ");
        gotoxy(wc,wr);   printf("  │  ★ Winner: %-14s │  ", c->winner);
        gotoxy(wc,wr+1); printf("  └───────────────────────────┘  ");
        attr_reset();
    }

    /* ── Opponent fields ─────────────────────────────────────────────────── */
    static const int PCOL_OPP[8][3] = {
        {0,200,255},{255,220,0},{180,0,255},{80,255,80},
        {255,60,60},{60,100,255},{255,140,0},{200,200,200}};
    {
        int opp_col_w = FIELD_WIDTH + 3;
        int opp_x0    = sx + 14;  /* right of sidebar */
        int opp_rows  = (FIELD_HEIGHT + 1) / 2;
        int col_idx   = 0;

        for (int i=0; i<MAX_PLAYERS; i++) {
            if (!c->opp_nicks[i][0]) continue;
            int alive = c->opp_alive[i];
            int ox2 = opp_x0 + col_idx * opp_col_w;
            int oy2 = fy - 1;
            if (ox2 + opp_col_w > cols) break;

            /* Nick header */
            gotoxy(ox2, oy2);
            fg(alive?180:70, alive?180:70, alive?255:90); attr_bold();
            printf("[%d] %.10s", i+1, c->opp_nicks[i]);
            attr_reset(); oy2++;

            /* Top border */
            gotoxy(ox2, oy2); fg(alive?60:40, alive?60:40, alive?100:60);
            printf("┌"); for(int c2=0;c2<FIELD_WIDTH;c2++) printf("─"); printf("┐");
            oy2++;

            /* Half-height field rows */
            for (int hrow=0; hrow<opp_rows; hrow++) {
                int row_top=hrow*2, row_bot=row_top+1;
                gotoxy(ox2, oy2);
                fg(alive?60:40,alive?60:40,alive?100:60); printf("│"); attr_reset();
                for (int col=0; col<FIELD_WIDTH; col++) {
                    uint8_t vt=c->opp_fields[i].cells[row_top][col];
                    uint8_t vb=(row_bot<FIELD_HEIGHT)?c->opp_fields[i].cells[row_bot][col]:0;
                    int rt,gt2,bt2,rb,gb,bb;
                    if(!vt){rt=alive?16:10;gt2=alive?16:10;bt2=alive?28:18;}
                    else if(vt>=8){rt=alive?180:60;gt2=alive?120:50;bt2=0;}
                    else{int*p=(int*)PCOL_OPP[(vt-1)%8];if(alive){rt=p[0];gt2=p[1];bt2=p[2];}else{rt=p[0]/4;gt2=p[1]/4;bt2=p[2]/4;}}
                    if(!vb){rb=alive?16:10;gb=alive?16:10;bb=alive?28:18;}
                    else if(vb>=8){rb=alive?180:60;gb=alive?120:50;bb=0;}
                    else{int*p=(int*)PCOL_OPP[(vb-1)%8];if(alive){rb=p[0];gb=p[1];bb=p[2];}else{rb=p[0]/4;gb=p[1]/4;bb=p[2]/4;}}
                    fg(rt,gt2,bt2); bg(rb,gb,bb); printf("▀"); attr_reset();
                }
                fg(alive?60:40,alive?60:40,alive?100:60); printf("│"); attr_reset();
                oy2++;
            }
            /* Bottom border */
            gotoxy(ox2, oy2); fg(alive?60:40,alive?60:40,alive?100:60);
            printf("└"); for(int c2=0;c2<FIELD_WIDTH;c2++) printf("─"); printf("┘");
            attr_reset();
            col_idx++;
        }
    }

    /* ── Split chat/activity panel (same as draw_in_room) ───────────────── */
    {
        int chat_top = rows - 11;
        if (chat_top < fy + FIELD_HEIGHT + 3) chat_top = fy + FIELD_HEIGHT + 3;
        int chat_rows = rows - chat_top - 4; /* header + blank + input + keyhints */
        if (chat_rows < 1) chat_rows = 1;
        int half = cols / 2;

        /* ROOM CHAT header + divider */
        gotoxy(0, chat_top); fg(60,60,180); attr_bold();
        printf("── ROOM CHAT "); for(int i=13;i<half-1;i++) printf("─");
        attr_reset();
        for (int row=chat_top; row<rows-1; row++) {
            gotoxy(half, row); fg(40,40,80); printf("│"); attr_reset();
        }
        /* Room chat lines */
        int rstart = c->chat_count > chat_rows ? c->chat_count - chat_rows : 0;
        for (int i=rstart; i<c->chat_count && i<rstart+chat_rows; i++) {
            gotoxy(0, chat_top+1+(i-rstart));
            char u8r[MAX_CHATLEN*2]; latin1_to_utf8(c->chat[i].text, u8r, sizeof(u8r));
            if (c->chat[i].is_server) { fg(120,120,200); printf("%-*.*s", half-1, half-1, u8r); }
            else { fg(100,100,180); int nl=(int)strlen(c->chat[i].nick)+3; printf("<%s> ",c->chat[i].nick);
                   fg(180,180,220); printf("%-*.*s", half-1-nl, half-1-nl, u8r); }
            attr_reset();
        }
        /* Input line */
        gotoxy(0, rows-2); fg(60,60,180); printf("> "); fg(200,200,255);
        if (c->chat_mode) printf("%s█", c->chat_input);
        else { fg(50,50,100); printf("Tab to chat..."); }
        attr_reset();

        /* ACTIVITY header */
        gotoxy(half+1, chat_top); fg(80,120,80); attr_bold();
        printf("── ACTIVITY "); for(int i=12;i<cols-half-2;i++) printf("─");
        attr_reset();
        /* Activity lines */
        int actw = cols - half - 2;
        int astart = c->gchat_count > chat_rows ? c->gchat_count - chat_rows : 0;
        for (int i=astart; i<c->gchat_count && i<astart+chat_rows; i++) {
            gotoxy(half+1, chat_top+1+(i-astart));
            char u8a[MAX_CHATLEN*2]; latin1_to_utf8(c->gchat[i].text, u8a, sizeof(u8a));
            if (c->gchat[i].is_speedup) { fg(255,220,0); attr_bold(); printf("%-*.*s", actw, actw, u8a); attr_reset(); }
            else if (c->gchat[i].is_server) { fg(120,180,120); printf("%-*.*s", actw, actw, u8a); }
            else { fg(100,150,100); int nl2=(int)strlen(c->gchat[i].nick)+3; printf("<%s> ",c->gchat[i].nick);
                   fg(180,220,180); printf("%-*.*s", actw-nl2, actw-nl2, u8a); }
            attr_reset();
        }
    }

    /* ── Key hints ───────────────────────────────────────────────────────── */
    gotoxy(0, rows-1); fg(60,60,100);
    if (c->state == CLI_PLAYING && !c->is_spectator)
        printf("← →=Move  ↑=Rotate  ↓=Drop  Spc=HardDrop  1-8=Fire at slot  Tab=Chat  Q=Leave");
    else
        printf("R=Ready  S=Start(host)  Tab=Chat  Q=Leave room");
    attr_reset();
    fflush(stdout);
}

static void redraw(CliCtx *c) {
    if (!c->dirty) return;
    c->dirty = 0;
    switch (c->state) {
        case CLI_CONNECT:   draw_connect_screen(c); break;
        case CLI_AUTH:      draw_connect_screen(c); break;
        case CLI_LOBBY_SEL: draw_lobby_select(c);   break;
        case CLI_IN_LOBBY:  draw_in_room(c);        break;
        case CLI_GAMEOVER:  draw_playing(c);        break; /* show field + watch */
        case CLI_PLAYING:   draw_playing(c);        break;
    }
}

/* ============================================================
 * Message handler
 * ============================================================ */
static void send_msg(CliCtx *c, const char *fmt, ...) {
    char buf[MAX_MSG_LEN];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    net_send(c->sock, buf);
}

static void handle_message(CliCtx *c, const char *line) {
    DLOG("[hm] entry sock=%d &sock=%p &c=%p line=%s\n",(int)c->sock,(void*)&c->sock,(void*)c,line);
    static char args[32][MAX_MSG_LEN];
    char cmd[64];
    DLOG("[hm] before proto_parse sock=%d &args=%p\n",(int)c->sock,(void*)args);
    int n = proto_parse(line, cmd, args, 32);
    DLOG("[hm] after proto_parse sock=%d\n",(int)c->sock);
    c->dirty = 1;
    DLOG("[msg] cmd=%s n=%d sock=%d\n", cmd, n, (int)c->sock);

    /* ---- Auth flow ---- */
    if (!strcmp(cmd,"noconnecting")) {
        /* If it carries a rejection reason (not the greeting), it is a disconnect */
        if (n>=2 && strcmp(args[0],"LineClash/1.0")!=0) {
            /* Terminated by administrator — show clearly */
            const char *why = args[0];
            char dmsg[256];
            if (strstr(why,"administrator"))
                snprintf(dmsg,sizeof(dmsg),"[!] Connection terminated by administrator");
            else
                snprintf(dmsg,sizeof(dmsg),"[!] Disconnected: %.200s",why);
            add_chat(c,"***",dmsg,1);
            c->dirty=1;
            return;
        }
        /* Server greeting — send authnick */
        send_msg(c, "authnick %s", c->nick);
        c->state = CLI_AUTH;
        return;
    }
    if (!strcmp(cmd,"authchallenge") && n>=2) {
        char resp[65];
        if (!strcmp(args[0],"new")) {
            c->is_new_account = 1;
            strncpy(resp, c->pass_hash, 65);
        } else {
            c->is_new_account = 0;
            snprintf(c->nonce, 65, "%.64s", args[0]);
            hmac_sha256_hex(c->pass_hash, c->nonce, resp);
        }
        send_msg(c, "authresp %s", resp);
        return;
    }
    if (!strcmp(cmd,"motd") || !strcmp(cmd,"servermotd")) {
        /* Show MOTD in server chat */
        if (n >= 2) {
            char motd_full[512]=""; int mi;
            for(mi=0;mi<n-1;mi++){if(mi)strncat(motd_full," ",sizeof(motd_full)-strlen(motd_full)-1);
                strncat(motd_full,args[mi],sizeof(motd_full)-strlen(motd_full)-1);}
            add_gchat(c,"MOTD",motd_full,1);
        }
        return;
    }
    if (!strcmp(cmd,"authok")) {
        /* Tell server our client type/platform */
#if defined(__APPLE__)
        send_msg(c, "clientinfo cli macos " TETRINETZ_VERSION);
#elif defined(_WIN32)
        send_msg(c, "clientinfo cli windows " TETRINETZ_VERSION);
#else
        send_msg(c, "clientinfo cli linux " TETRINETZ_VERSION);
#endif
        /* Authenticated — request lobby list */
        send_msg(c, "listlobbies");
        send_msg(c, "reqleaderboard");
        c->state = CLI_LOBBY_SEL;
        /* Auto-save profile if connected without a saved alias */
        if (c->save_on_auth) {
            c->save_on_auth = 0;
            const char *alias = c->server_name[0] ? c->server_name : c->host;
            /* Only save if not already in profiles */
            if (!cli_profile_find(alias)) {
                if (g_nprofiles < CLI_PROFILES_MAX) {
                    CliProfile *p = &g_profiles[g_nprofiles++];
                    memset(p, 0, sizeof(*p));
                    snprintf(p->alias, sizeof(p->alias), "%.63s", alias);
                    snprintf(p->host,  sizeof(p->host),  "%.255s", c->host);
                    p->port = c->port;
                    snprintf(p->nick,  MAX_NICK_LEN, "%.31s", c->nick);
                    snprintf(p->hash,  65, "%.64s", c->pass_hash);
                    cli_profiles_save();
                }
            }
        }
        return;
    }
    if (!strcmp(cmd,"autherr")) {
        add_gchat(c, "***", args[0][0] ? args[0] : "Authentication failed", 1);
        return;
    }
    if (!strcmp(cmd,"privlevel") && n>=2) {
        c->priv_level = atoi(args[0]);
        return;
    }

    /* ---- Lobby list ---- */
    if ((!strcmp(cmd,"lobbyinfo") || !strcmp(cmd,"lobbyupdate")) && n>=9) {
        int id=atoi(args[0]), idx=c->room_count, i;
        for(i=0;i<c->room_count;i++) if(c->rooms[i].id==id){idx=i;break;}
        if(idx==c->room_count&&c->room_count<MAX_LOBBIES) c->room_count++;
        RoomInfo *r = &c->rooms[idx];
        r->id=id; r->players=atoi(args[1]); r->max_players=atoi(args[2]);
        r->in_game=(strcmp(args[3],"ingame")==0);
        r->no_attacks=atoi(args[4]); r->last_player_wins=atoi(args[5]);
        r->no_taunts=atoi(args[6]); r->paused=atoi(args[7]);
        r->speedup_enabled=(n>9)?atoi(args[8]):1;
        r->garbage_system  =(n>10)?atoi(args[9]):0;
        r->garbage_targeting=(n>11)?atoi(args[10]):0;
        r->hold_enabled    =(n>12)?atoi(args[11]):1;
        /* name is at args[12] onward */
        c->dirty=1;  /* refresh lobby list immediately */
        return;
    }
    if (!strcmp(cmd,"lobbylistend")) { return; }

    /* ---- Joining a room ---- */
    if (!strcmp(cmd,"playernum") && n>=2) {
        c->slot = atoi(args[0]);
        c->attack_target = c->slot;
        c->state = CLI_IN_LOBBY; /* server confirmed join */
        return;
    }
    if (!strcmp(cmd,"host") && n>=2) {
        c->host_slot = atoi(args[0]);
        c->is_host = (c->host_slot == c->slot);
        return;
    }
    if (!strcmp(cmd,"lobbysettings") && n>=4) {
        c->no_attacks=atoi(args[1]); c->last_player_wins=atoi(args[2]);
        if (n>=5) c->no_taunts=atoi(args[3]);
        if (n>=6) c->speedup_enabled=atoi(args[4]);
        if (n>=7) c->garbage_system=atoi(args[5]);
        if (n>=8) c->garbage_targeting=atoi(args[6]);
        if (n>=9) c->hold_enabled=atoi(args[7]);
        return;
    }
    if (!strcmp(cmd,"yourteam") && n>=2) {
        const char *t=args[0]; if(strcmp(t,"-")==0) t="";
        strncpy(c->team, t, MAX_NICK_LEN-1); c->team[MAX_NICK_LEN-1]='\0';
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"playerjoin") && n>=3) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS&&sl!=c->slot-1) {
            /* Only announce in chat if this is a genuinely new player */
            int is_new = (c->opp_nicks[sl][0] == '\0');
            snprintf(c->opp_nicks[sl], MAX_NICK_LEN, "%.31s", args[1]);
            c->opp_alive[sl] = 0; /* reset alive for lobby state */
            if (is_new) {
                char msg[MAX_MSG_LEN]; snprintf(msg,sizeof(msg),"%.31s joined the room",args[1]);
                add_chat(c,"***",msg,1);
            }
        }
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"playerleave") && n>=2) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS) {
            char msg[128]; snprintf(msg,sizeof(msg),"%s left the room",c->opp_nicks[sl]);
            add_chat(c,"***",msg,1);
            memset(c->opp_nicks[sl],0,MAX_NICK_LEN);
            c->opp_alive[sl]=0; c->opp_ready[sl]=0;
            c->opp_is_bot[sl]=0; c->opp_bot_diff[sl]=0;
        }
        return;
    }
    if (!strcmp(cmd,"team") && n>=3) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS) snprintf(c->opp_teams[sl], MAX_NICK_LEN, "%.31s", args[1]);
        return;
    }
    if (!strcmp(cmd,"readystatus") && n>=3) {
        int sl=atoi(args[0])-1;
        int rdy=atoi(args[1]);
        if(sl>=0&&sl<MAX_PLAYERS) c->opp_ready[sl]=rdy;
        /* Also sync own ready state — server is authoritative */
        if(sl+1==c->slot) c->is_ready=rdy;
        return;
    }

    /* ---- Game messages ---- */
    if (!strcmp(cmd,"newgame")) {
        int freq[NUM_PIECES]={14,14,14,14,14,14,14};
        if(n>=2) c->field.level=atoi(args[0]);
        if(c->field.level<1) c->field.level=1;
        c->state = CLI_PLAYING;
        c->alive = 1; c->is_ready=0; c->paused=0;
        c->winner[0]='\0';
        c->hold_type=-1; c->hold_used=0;
        c->lock_active=0; c->lock_timer=0;
        c->spec_head=c->spec_tail=0;
        c->tlb_count=0;
        c->cur.type=-1;  /* sentinel: no piece yet */
        field_init(&c->field);
        for(int i=0;i<MAX_PLAYERS;i++){
            field_init(&c->opp_fields[i]);
            c->opp_alive[i]=!!c->opp_nicks[i][0];
            c->opp_ready[i]=0;
        }
        if(!c->is_spectator){
            piece_random(&c->cur, freq);
            piece_random(&c->next, freq);
            piece_spawn(&c->field, &c->cur);
            c->drop_interval = level_to_interval(c->field.level);
            if(c->drop_interval<2) c->drop_interval=2;
            c->drop_timer = c->drop_interval;
        }
        add_chat(c,"***","Game started!",1);
        return;
    }
    if (!strcmp(cmd,"playerwon") && n>=2) {
        snprintf(c->winner, MAX_NICK_LEN, "%.31s", args[0]);
        char msg[MAX_MSG_LEN]; snprintf(msg,sizeof(msg),"★ %.31s wins!",args[0]);
        add_chat(c,"***",msg,1);
        return;
    }
    if (!strcmp(cmd,"playerlost") && n>=2) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS) {
            c->opp_alive[sl]=0;
            if(sl==c->slot-1) { c->alive=0; }
        }
        return;
    }
    if (!strcmp(cmd,"endgame") || !strcmp(cmd,"ingame")) {
        /* Only act if we are still in the room — ignore stale messages */
        if(c->state==CLI_PLAYING || c->state==CLI_IN_LOBBY || c->state==CLI_GAMEOVER) {
            c->state=CLI_IN_LOBBY;
            c->alive=0;
            c->is_ready=0;
            memset(c->opp_ready,0,sizeof(c->opp_ready));
            /* Clear opp_nicks so playerjoin re-sync populates them cleanly */
            /* Don't clear — we keep names, server will re-send playerjoin */
            if(!strcmp(cmd,"endgame")) add_chat(c,"***","Game over.",1);
        }
        return;
    }
    if (!strcmp(cmd,"f") && n>=3) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS&&sl!=c->slot-1) {
            field_decode(&c->opp_fields[sl], args[1]);
            c->opp_alive[sl]=1;
            c->dirty=1;
        }
        return;
    }
    if (!strcmp(cmd,"fieldack") && n>=4) {
        int srv_lines=atoi(args[0]),srv_score=atoi(args[1]),srv_level=atoi(args[2]);
        c->field.lines=srv_lines; c->field.score=srv_score;
        if(srv_level!=c->field.level) {
            c->field.level=srv_level;
            c->drop_interval=level_to_interval(srv_level);
            if(c->drop_interval<2)c->drop_interval=2;
            send_msg(c,"lvl %d %d",c->slot,srv_level);
        }
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"lvl") && n>=3) {
        int sl=atoi(args[0])-1;
        int lv=atoi(args[1]);
        if(sl>=0&&sl<MAX_PLAYERS) {
            if(sl==c->slot-1) { c->field.level=lv;
                c->drop_interval=level_to_interval(lv);
                if(c->drop_interval<2) c->drop_interval=2; }
            else c->opp_level[sl]=lv;
        }
        return;
    }
    if (!strcmp(cmd,"sb") && n>=4) {
        int to=atoi(args[0]); char sp_char=args[1][0];
        uint8_t sp=0;
        for(int si=0;SPECIAL_CHARS[si];si++) if(SPECIAL_CHARS[si]==sp_char){sp=(uint8_t)(si+1);break;}
        if(to==c->slot && !c->is_spectator && sp>0) {
            switch(sp){
                case SP_ADDLINE:      field_add_garbage(&c->field,1); break;
                case SP_CLEARLINE:    field_clear_line_bottom(&c->field); break;
                case SP_CLEARSPECIAL: field_clear_specials(&c->field); break;
                case SP_RANDOMCLEAR:  field_random_clear(&c->field); break;
                case SP_BLOCKBOMB:    field_block_bomb(&c->field,&c->cur); break;
                case SP_QUAKEFIELD:   field_quake(&c->field); break;
                case SP_GRAVITY:      field_gravity(&c->field); break;
                case SP_NUKEFIELD:    field_nuke(&c->field); break;
                case SP_CLEARCOLUMN:  field_clear_column(&c->field); break;
                case SP_SWITCHFIELD:  break; /* server-side; myfield arrives separately */
                default: break;
            }
            if(sp!=SP_SWITCHFIELD && sp!=SP_IMMUNITY && sp!=SP_MUTATE &&
               sp!=SP_DARKNESS && sp!=SP_CONFUSION)
                proto_send_field(c->sock, c->slot, &c->field);
            c->dirty = 1;
        }
        /* Log ALL specials to activity panel */
        if(sp>0) {
            static const char *SP_NAMES[]={"AddLine","ClearLine","ClearSpec","RandomClear",
                "BlockBomb","ClearCol","Nuke","Quake","Switch","Immunity","Dark","Confuse","Mutate","Gravity"};
            int from_sl=atoi(args[2]);
            char from_nick[32]="?", to_nick[32]="?";
            if(from_sl==c->slot) strncpy(from_nick,c->nick,31);
            else for(int pi=0;pi<MAX_PLAYERS;pi++) if(c->opp_nicks[pi][0]&&pi+1==from_sl)
                { strncpy(from_nick,c->opp_nicks[pi],31); break; }
            if(to==c->slot) strncpy(to_nick,"you",31);
            else for(int pi=0;pi<MAX_PLAYERS;pi++) if(c->opp_nicks[pi][0]&&pi+1==to)
                { strncpy(to_nick,c->opp_nicks[pi],31); break; }
            char act[80];
            const char *spname=(sp>=1&&sp<=14)?SP_NAMES[sp-1]:"?";
            if(from_sl==to) snprintf(act,sizeof(act),"%s: %s (self)",from_nick,spname);
            else            snprintf(act,sizeof(act),"%s->%s: %s",from_nick,to_nick,spname);
            add_gchat(c,"",act,1);
        }
        return;
    }
    if (!strcmp(cmd,"pause") && n>=3) {
        int is_paused = atoi(args[0]);
        c->paused = is_paused;
        char msg[64]; snprintf(msg,sizeof(msg),"Game %s",is_paused?"paused":"resumed");
        add_chat(c,"***",msg,1);
        c->dirty = 1;
        return;
    }

    /* ---- Leaderboard ---- */
    if (!strcmp(cmd,"nextpiece") && n>=2) {
        char pc=args[0][0]; int ptype=-1;
        for(int pi=0;pi<NUM_PIECES;pi++)
            if(PIECE_CHARS[pi]==pc){ptype=pi;break;}
        if(ptype>=0 && c->srv_piece_tail-c->srv_piece_head<126)
            c->srv_pieces[c->srv_piece_tail++%128]=ptype;
        return;
    }
    if (!strcmp(cmd,"botinfo") && n>=3) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS){
            c->opp_is_bot[sl]=1;
            c->opp_bot_diff[sl] = !strcasecmp(args[1],"hard")?2:
                                   !strcasecmp(args[1],"medium")?1:0;
        }
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"botsallowed") && n>=2) {
        c->bots_allowed=atoi(args[0]); return;
    }
    if (!strcmp(cmd,"addspecial") && n>=2) {
        char sp_char = args[0][0];
        int si; uint8 sp=0;
        for(si=0;SPECIAL_CHARS[si];si++) if(SPECIAL_CHARS[si]==sp_char){sp=(uint8)(si+1);break;}
        if (sp) field_add_special(&c->field, sp);
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"lockdelay") && n>=2) {
        c->lock_delay_ms=atoi(args[0]);
        if(c->lock_delay_ms<0)c->lock_delay_ms=0;
        if(c->lock_delay_ms>500)c->lock_delay_ms=500;
        return;
    }
    if (!strcmp(cmd,"myfield") && n>=2) {
        field_decode(&c->field, args[0]);
        piece_spawn(&c->field, &c->cur);
        proto_send_field(c->sock, c->slot, &c->field);
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"addline") && n>=2) {
        /* Speed-up: server pushes garbage lines onto our field */
        int nl = atoi(args[0]);
        if (nl > 0 && nl <= 4) { field_add_garbage(&c->field, nl); c->dirty=1; }
        return;
    }
    if (!strcmp(cmd,"speedup") && n>=2) {
        /* Speed-up notification — bright yellow in activity log */
        char msg[MAX_CHATLEN]="";
        for(int si2=0;si2<n-1;si2++){
            if(si2>0) strncat(msg," ",sizeof(msg)-strlen(msg)-1);
            strncat(msg,args[si2],sizeof(msg)-strlen(msg)-1);
        }
        /* Add to gchat with speedup flag */
        if (c->gchat_count >= MAX_CHAT) {
            memmove(c->gchat, c->gchat+1, sizeof(ChatLine)*(MAX_CHAT-1));
            c->gchat_count = MAX_CHAT-1;
        }
        ChatLine *sl = &c->gchat[c->gchat_count++];
        strncpy(sl->nick,"***",MAX_NICK_LEN-1);
        strncpy(sl->text,msg,MAX_CHATLEN-1);
        sl->is_server=1; sl->is_speedup=1;
        c->dirty=1;
        return;
    }
    if (!strcmp(cmd,"leaderboard") && n>=3) {
        c->round_num=atoi(args[0]); c->lb_count=atoi(args[1]);
        if(c->lb_count>MAX_PLAYERS) c->lb_count=MAX_PLAYERS;
        for(int i=0;i<c->lb_count&&i*3+3<n;i++) {
            snprintf(c->lb_nicks[i], MAX_NICK_LEN, "%.31s", args[i*3+2]);
            c->lb_wins[i]=atoi(args[i*3+3]);
            c->lb_streaks[i]=(i*3+4<n)?atoi(args[i*3+4]):0;
        }
        return;
    }
    if (!strcmp(cmd,"teamleaderboard") && n>=2) {
        c->tlb_count = atoi(args[0]);
        if(c->tlb_count>MAX_PLAYERS) c->tlb_count=MAX_PLAYERS;
        for(int i=0;i<c->tlb_count&&i*2+2<n&&i*2+2<32;i++) {
            snprintf(c->tlb_teams[i],MAX_NICK_LEN,"%.31s",args[i*2+1]);
            c->tlb_wins[i]=atoi(args[i*2+2]);
        }
        return;
    }

    /* ---- Chat ---- */
    if (!strcmp(cmd,"pline") && n>=3) {
        /* Reconstruct message */
        char msg[MAX_CHATLEN]="";
        for(int i=1;i<n-1&&i<14;i++){
            if(i>1) strncat(msg," ",sizeof(msg)-strlen(msg)-1);
            strncat(msg,args[i],sizeof(msg)-strlen(msg)-1);
        }
        {char pmsg[MAX_CHATLEN]; chat_process(msg,pmsg,sizeof(pmsg));
         add_chat(c, args[0], pmsg, 0);}
        return;
    }
    if (!strcmp(cmd,"gmsg") && n>=2) {
        /* Reconstruct full gmsg */
        char msg[MAX_CHATLEN]="";
        for(int i=0;i<n-1;i++){
            if(i>0) strncat(msg," ",sizeof(msg)-strlen(msg)-1);
            strncat(msg,args[i],sizeof(msg)-strlen(msg)-1);
        }
        {char pmsg[MAX_CHATLEN]; chat_process(msg,pmsg,sizeof(pmsg));
         add_chat(c,"***",pmsg,1); add_gchat(c,"***",pmsg,1);}
        return;
    }
    if (!strcmp(cmd,"schat") && n>=3) {
        char msg[MAX_CHATLEN]="";
        for(int i=1;i<n-1;i++){
            if(i>1) strncat(msg," ",sizeof(msg)-strlen(msg)-1);
            strncat(msg,args[i],sizeof(msg)-strlen(msg)-1);
        }
        {char pmsg[MAX_CHATLEN]; chat_process(msg,pmsg,sizeof(pmsg));
         add_gchat(c, args[0], pmsg, 0);}
        return;
    }

    /* ---- Server chat ---- */
    if (!strcmp(cmd,"servername") && n>=2) {
        snprintf(c->server_name, 64, "%.63s", args[0]);
        return;
    }
    if (!strcmp(cmd,"kicked") && n>=2) {
        char msg[MAX_MSG_LEN]; snprintf(msg,sizeof(msg),"*** KICKED: %.31s",args[0]);
        add_gchat(c,"***",msg,1);
        c->state=CLI_LOBBY_SEL;
        send_msg(c,"listlobbies");
        return;
    }
    if (!strcmp(cmd,"taunt") && n>=3) {
        char msg[MAX_MSG_LEN];
        snprintf(msg, sizeof(msg), "%.31s taunts %.31s!", args[0], args[1]);
        add_chat(c, "***", msg, 1);
        return;
    }
    if (!strcmp(cmd,"serverteamleaderboard") ||
        !strcmp(cmd,"serverleaderboard") ||
        !strcmp(cmd,"spectator")) { return; }
    /* lobbyupdate: parse inline so lobby list stays fresh while in room */
    if (!strcmp(cmd,"lobbyupdate") && n>=9) {
        int id=atoi(args[0]), idx=0, i;
        for(i=0;i<c->room_count;i++) if(c->rooms[i].id==id){idx=i;break;}
        RoomInfo *r2 = &c->rooms[idx];
        r2->id=id; r2->players=atoi(args[1]); r2->max_players=atoi(args[2]);
        r2->in_game=(strcmp(args[3],"ingame")==0);
        r2->no_attacks=atoi(args[4]); r2->last_player_wins=atoi(args[5]);
        r2->no_taunts=atoi(args[6]); r2->paused=atoi(args[7]);
        r2->speedup_enabled=(n>9)?atoi(args[8]):1;
        r2->garbage_system  =(n>10)?atoi(args[9]):0;
        r2->garbage_targeting=(n>11)?atoi(args[10]):0;
        r2->hold_enabled   =(n>12)?atoi(args[11]):1;
        return;
    }
}

/* ============================================================
 * Network receive
 * ============================================================ */
static void net_tick(CliCtx *c) {
    char line[MAX_MSG_LEN * 4]; /* extra large to prevent any overflow */
    int r, got_close = 0;
    DLOG("[net_tick] start sock=%d recv_len=%d\n", (int)c->sock, c->recv_len);
    static sock_t prev_sock = INVALID_SOCK;
    if (prev_sock != INVALID_SOCK && c->sock != prev_sock)
        DLOG("[net_tick] !!! SOCK CHANGED from %d to %d\n", (int)prev_sock, (int)c->sock);
    prev_sock = c->sock;
    /* Keep draining even after graceful close — server may have sent data+FIN */
    while (1) {
        r = net_recv(c->sock, c->recv_buf, &c->recv_len, line, sizeof(line));
        if (r == 1) {
            DLOG("[net_tick] recv: %s\n", line);
            if (line[0]) handle_message(c, line);
            DLOG("[net_tick] after handle_message, state=%d sock=%d\n", c->state, (int)c->sock);
        } else if (r == -1) {
            DLOG("[net_tick] r=-1 errno=%d recv_len=%d got_close=%d sock=%d buf=[%.*s]\n",
                 errno, c->recv_len, got_close, (int)c->sock, c->recv_len, c->recv_buf);
            if (got_close) break;
            got_close = 1;
            if (c->recv_len == 0) break;
        } else {
            break; /* r==0: EAGAIN, no more data right now */
        }
    }
    if (got_close && c->recv_len == 0) {
        /* Connection closed and buffer empty — disconnect */
        DLOG("[net_tick] connection closed by server\n");
        c->running = 0;
    }
    DLOG("[net_tick] done, r=%d recv_len=%d got_close=%d sock=%d\n", r, c->recv_len, got_close, (int)c->sock);
    DLOG("[net_tick] returning now\n");
}

/* ============================================================
 * Input handling
 * ============================================================ */
static int read_key(char *buf, int maxlen) {
    int n = (int)read(STDIN_FILENO, buf, maxlen-1);
    if (n > 0) buf[n] = '\0';
    return n;
}

static void handle_input_lobby(CliCtx *c, const char *buf, int n) {
    (void)n;
    if (c->chat_mode) {
        if (buf[0] == '\r' || buf[0] == '\n') {
            if (c->chat_input[0]) {
                char l1[MAX_CHATLEN]; utf8_to_latin1(c->chat_input,l1,sizeof(l1));
                send_msg(c, "schat %s", l1);
                c->chat_input[0] = '\0';
            }
            c->chat_mode = 0;
        } else if (buf[0] == '\t') {
            c->chat_mode = 0;
        } else if (buf[0] == 127 || buf[0] == '\b') {
            str_backspace_utf8(c->chat_input);
        } else if ((unsigned char)buf[0] >= 32) {
            /* Accept ASCII and multi-byte UTF-8 sequences */
            int l = strlen(c->chat_input), bi = 0;
            while (buf[bi] && l < (int)sizeof(c->chat_input)-2) {
                /* Skip escape sequences (arrow keys etc) that slipped through */
                if ((unsigned char)buf[bi] == 0x1B) break;
                c->chat_input[l++] = buf[bi++];
            }
            c->chat_input[l] = '\0';
        }
        c->dirty = 1;
        return;
    }
    /* Escape sequences */
    if (buf[0] == '\033' && buf[1] == '[') {
        if (buf[2] == 'A') { /* Up */
            if (c->room_cursor > 0) c->room_cursor--;
        } else if (buf[2] == 'B') { /* Down */
            if (c->room_cursor < c->room_count-1) c->room_cursor++;
        }
        c->dirty = 1;
        return;
    }
    char k = tolower(buf[0]);
    if (k == '\r' || k == '\n' || k == 'j') {
        /* Join selected room — block if full or in-game */
        if (c->room_count > 0) {
            RoomInfo *r = &c->rooms[c->room_cursor];
            int is_full2 = (r->max_players > 0 && r->players >= r->max_players);
            if (is_full2) {
                add_gchat(c,"***","Room is FULL — cannot join.",1);
                c->dirty = 1; return;
            }
            send_msg(c, "lineclash_join %s %s %d", c->nick,
                     c->team[0] ? c->team : "-", r->id);
            c->room_id = r->id;
            /* Stay in CLI_LOBBY_SEL until server confirms with playernum */
            memset(c->opp_nicks, 0, sizeof(c->opp_nicks));
            memset(c->opp_alive, 0, sizeof(c->opp_alive));
            memset(c->opp_ready, 0, sizeof(c->opp_ready));
            c->lb_count = 0; c->round_num = 0;
        }
    } else if (k == '\t') {
        c->chat_mode = 1;
        c->chat_input[0] = '\0';
    } else if (k == 'q' || k == 3 /* Ctrl-C */) {
        c->running = 0;
    }
    c->dirty = 1;
}

static void handle_input_room(CliCtx *c, const char *buf, int n) {
    (void)n;
    if (c->chat_mode) {
        if (buf[0] == '\r' || buf[0] == '\n') {
            if (c->chat_input[0]) {
                char l1[MAX_CHATLEN]; utf8_to_latin1(c->chat_input,l1,sizeof(l1));
                send_msg(c, "pline %d %s", c->slot, l1);
                c->chat_input[0] = '\0';
            }
            c->chat_mode = 0;
        } else if (buf[0] == '\t') {
            c->chat_mode = 0;
        } else if (buf[0] == 127 || buf[0] == '\b') {
            str_backspace_utf8(c->chat_input);
        } else if ((unsigned char)buf[0] >= 32) {
            int l = strlen(c->chat_input), bi = 0;
            while (buf[bi] && l < (int)sizeof(c->chat_input)-2) {
                if ((unsigned char)buf[bi] == 0x1B) break;
                c->chat_input[l++] = buf[bi++];
            }
            c->chat_input[l] = '\0';
        }
        c->dirty = 1;
        return;
    }
    /* Team input mode */
    if (c->team_input_mode) {
        if (buf[0] == '\r' || buf[0] == '\n') {
            /* Send team — empty string clears it */
            char l1[MAX_NICK_LEN]; utf8_to_latin1(c->chat_input,l1,sizeof(l1));
            send_msg(c, "setteam %s", l1[0] ? l1 : "-");
            strncpy(c->team, l1, MAX_NICK_LEN-1);
            c->chat_input[0] = '\0';
            c->team_input_mode = 0;
        } else if (buf[0] == '\t' || buf[0] == 27) {
            c->team_input_mode = 0;
            c->chat_input[0] = '\0';
        } else if (buf[0] == 127 || buf[0] == '\b') {
            str_backspace_utf8(c->chat_input);
        } else if ((unsigned char)buf[0] >= 32) {
            int l = strlen(c->chat_input);
            if (l < 8) { /* max team length */
                c->chat_input[l] = buf[0];
                c->chat_input[l+1] = '\0';
            }
        }
        c->dirty = 1;
        return;
    }

    char k = tolower(buf[0]);
    if (k == 'r' && c->state == CLI_IN_LOBBY) {
        c->is_ready = !c->is_ready;
        send_msg(c, "playerready");
        c->dirty = 1;
    } else if (k == 's' && c->is_host && c->state == CLI_IN_LOBBY) {
        send_msg(c, "startgame 1");
    } else if (k == 'e' && c->bots_allowed &&
               (c->is_host||c->priv_level<=2) && c->state==CLI_IN_LOBBY) {
        { int np=1; for(int bi=0;bi<MAX_PLAYERS;bi++) if(c->opp_nicks[bi][0]) np++;
          if(np>=MAX_PLAYERS) add_gchat(c,"***","Room full (8/8) — cannot add bot.",1);
          else send_msg(c, "addbot easy"); }
    } else if (k == 'm' && c->bots_allowed &&
               (c->is_host||c->priv_level<=2) && c->state==CLI_IN_LOBBY) {
        { int np=1; for(int bi=0;bi<MAX_PLAYERS;bi++) if(c->opp_nicks[bi][0]) np++;
          if(np>=MAX_PLAYERS) add_gchat(c,"***","Room full (8/8) — cannot add bot.",1);
          else send_msg(c, "addbot medium"); }
    } else if (k == 'h' && c->bots_allowed &&
               (c->is_host||c->priv_level<=2) && c->state==CLI_IN_LOBBY) {
        { int np=1; for(int bi=0;bi<MAX_PLAYERS;bi++) if(c->opp_nicks[bi][0]) np++;
          if(np>=MAX_PLAYERS) add_gchat(c,"***","Room full (8/8) — cannot add bot.",1);
          else send_msg(c, "addbot hard"); }
    } else if (k == 'x' && c->bots_allowed &&
               (c->is_host||c->priv_level<=2) && c->state==CLI_IN_LOBBY) {
        /* Remove first bot found */
        for(int bi=0;bi<MAX_PLAYERS;bi++){
            if(c->opp_is_bot[bi]){
                send_msg(c,"removebot %d",bi+1);
                break;
            }
        }
    } else if ((k == '1' || k == '2' || k == '3' || k == '4' || k == '5' || k == '6') &&
               (c->is_host || c->priv_level <= 2) && c->state == CLI_IN_LOBBY) {
        /* 1=Specials 2=Last-Wins 3=Taunts 4=Speed-Up 5=Garbage 6=Target */
        if (k == '1') c->no_attacks     = !c->no_attacks;
        else if (k == '2') c->last_player_wins = !c->last_player_wins;
        else if (k == '3') c->no_taunts      = !c->no_taunts;
        else if (k == '4') c->speedup_enabled = !c->speedup_enabled;
        else if (k == '5') c->garbage_system  = c->garbage_system ? 0 : 1;
        else if (k == '6') c->garbage_targeting = (c->garbage_targeting + 1) % 3;
        send_msg(c, "setsettings %d %d %d %d %d %d",
                 c->no_attacks, c->last_player_wins, c->no_taunts, c->speedup_enabled,
                 c->garbage_system, c->garbage_targeting);
        c->dirty = 1;
    } else if (k == 't' && c->state == CLI_IN_LOBBY) {
        c->team_input_mode = 1;
        /* Pre-fill with current team */
        strncpy(c->chat_input, c->team, sizeof(c->chat_input)-1);
        c->dirty = 1;
    } else if (k == '\t') {
        c->chat_mode = 1;
        c->chat_input[0] = '\0';
        c->dirty = 1;
    } else if (k == 'q' || k == 3) {
        send_msg(c, "leaveLobby");
        { struct timespec ts={0,50000000L}; nanosleep(&ts,NULL); }
        c->state = CLI_LOBBY_SEL;
        c->room_id = 0; c->slot = 0;
        c->is_host = 0; c->is_spectator = 0;
        c->is_ready = 0;
        c->room_count=0; c->room_cursor=0;
        c->lb_count = 0; c->winner[0] = '\0';
        c->spec_watch_slot = 0;
        c->team_input_mode = 0;
        memset(c->opp_nicks,0,sizeof(c->opp_nicks));
        memset(c->chat,0,sizeof(c->chat)); c->chat_count=0;
        send_msg(c, "listlobbies");
        c->dirty = 1;
    }
}

static void handle_input_playing(CliCtx *c, const char *buf, int n) {
    (void)n;
    if (c->chat_mode) {
        if (buf[0] == '\r' || buf[0] == '\n') {
            if (c->chat_input[0]) {
                char l1[MAX_CHATLEN]; utf8_to_latin1(c->chat_input,l1,sizeof(l1));
                send_msg(c, "pline %d %s", c->slot, l1);
                c->chat_input[0] = '\0';
            }
            c->chat_mode = 0;
        } else if (buf[0] == '\t') { c->chat_mode = 0; }
        else if (buf[0] == 127 || buf[0] == '\b') {
            int l = strlen(c->chat_input);
            if (l > 0) c->chat_input[l-1] = '\0';
        } else if (buf[0] >= 32 && buf[0] < 127) {
            int l = strlen(c->chat_input);
            if (l < (int)sizeof(c->chat_input)-2) {
                c->chat_input[l]=buf[0]; c->chat_input[l+1]='\0';
            }
        }
        c->dirty = 1; return;
    }
    /* Escape sequences for arrow keys */
    if (buf[0] == '\033' && buf[1] == '[') {
        if (buf[2] == 'A') { /* Up = rotate CW */
            if (c->alive && c->cur.type>=0) {
                Piece np=c->cur;
                if (piece_rotate_cw(&c->field,&np)) {
                    c->cur=np;
                    c->lock_timer=c->lock_delay_ms; /* reset lock on rotate */
                    proto_send_field(c->sock, c->slot, &c->field);
                }
            }
        } else if (buf[2] == 'B') { /* Down = soft drop */
            if (c->alive && c->cur.type>=0) {
                if (!piece_move_down(&c->field, &c->cur)) {
                    if (c->lock_delay_ms > 0) { c->lock_active=1; c->lock_timer=c->lock_delay_ms; }
                    else { lock_and_spawn(c); net_send(c->sock,"holdclear"); }
                } else { c->drop_timer=c->drop_interval; c->lock_active=0;
                         proto_send_field(c->sock, c->slot, &c->field); c->dirty=1; }
            }
        } else if (buf[2] == 'C') { /* Right — move or cycle watch target */
            if (!c->alive || c->is_spectator) {
                /* Cycle to next alive opponent */
                int cur = c->spec_watch_slot, found = 0;
                for (int di=1; di<=MAX_PLAYERS; di++) {
                    int try_s = (cur % MAX_PLAYERS) + di;
                    if (try_s > MAX_PLAYERS) try_s -= MAX_PLAYERS;
                    if (try_s == c->slot) continue;
                    int idx = try_s - 1;
                    if (c->opp_nicks[idx][0]) {
                        c->spec_watch_slot = try_s; found=1; break;
                    }
                }
                if (!found) c->spec_watch_slot = 0;
            } else if (c->cur.type>=0) {
                if (piece_move_right(&c->field, &c->cur)) {
                    c->lock_timer=c->lock_delay_ms;
                    proto_send_field(c->sock, c->slot, &c->field);
                }
            }
        } else if (buf[2] == 'D') { /* Left — move or cycle watch target */
            if (!c->alive || c->is_spectator) {
                int cur = c->spec_watch_slot;
                if (cur == 0) cur = c->slot;
                for (int di=1; di<=MAX_PLAYERS; di++) {
                    int try_s = cur - di;
                    if (try_s < 1) try_s += MAX_PLAYERS;
                    if (try_s == c->slot) continue;
                    int idx = try_s - 1;
                    if (c->opp_nicks[idx][0]) {
                        c->spec_watch_slot = try_s; break;
                    }
                }
            } else if (c->cur.type>=0) {
                if (piece_move_left(&c->field, &c->cur)) {
                    c->lock_timer=c->lock_delay_ms;
                    proto_send_field(c->sock, c->slot, &c->field);
                }
            }
        }
        c->dirty = 1; return;
    }
    char k = tolower(buf[0]);
    if (k == ' ') { /* Hard drop */
        if (c->alive && c->cur.type>=0) {
            piece_drop(&c->field, &c->cur);
            c->lock_active=0;
            lock_and_spawn(c);
            net_send(c->sock,"holdclear");
        }
    } else if (k == 'c') { /* Hold piece */
        if (c->hold_enabled && c->alive && c->cur.type>=0 && !c->hold_used) {
            c->hold_used = 1;
            c->lock_active = 0; c->lock_timer = 0;
            int prev = c->hold_type;
            c->hold_type = c->cur.type;
            { char hbuf[32]; snprintf(hbuf,sizeof(hbuf),"holdpiece %d",c->hold_type);
              net_send(c->sock,hbuf); }
            if (prev >= 0) {
                piece_init(&c->cur, prev);
                piece_spawn(&c->field, &c->cur);
            } else {
                /* Pull next piece */
                if (c->srv_piece_head < c->srv_piece_tail)
                    piece_init(&c->cur, c->srv_pieces[c->srv_piece_head++ % 128]);
                else {
                    int freq[NUM_PIECES]={14,14,14,14,14,14,14};
                    piece_random(&c->cur, freq);
                }
                piece_spawn(&c->field, &c->cur);
            }
            c->drop_timer = c->drop_interval;
            proto_send_field(c->sock, c->slot, &c->field);
            c->dirty = 1;
        }
    } else if (k == 'z') { /* Rotate CCW */
        if (c->alive && c->cur.type>0) {
            if (c->cur.type>=0) {
            Piece np=c->cur;
            if (piece_rotate_ccw(&c->field,&np)) { c->cur=np; proto_send_field(c->sock,c->slot,&c->field); }
        }
        }
    } else if (buf[0] >= '1' && buf[0] <= '8') {
        /* Fire special: key 1=own field, 2-8=opponents in display order */
        if (!c->no_attacks) {
            int disp_key = buf[0] - '0';
            int target_slot = -1;
            if (disp_key == 1) {
                target_slot = c->slot; /* self */
            } else {
                /* Find the (disp_key-1)th opponent in slot order */
                int opp_n = 0, oi;
                for (oi = 0; oi < MAX_PLAYERS; oi++) {
                    if (oi + 1 == c->slot) continue;
                    if (c->opp_nicks[oi][0]) {
                        opp_n++;
                        if (opp_n == disp_key - 1) { target_slot = oi + 1; break; }
                    }
                }
            }
            if (target_slot > 0 && c->field.special_count > 0) {
                uint8_t sp = field_pop_special(&c->field);
                if (sp > 0) {
                    proto_send_field(c->sock, c->slot, &c->field);
                    proto_send_special(c->sock, c->slot, sp, target_slot);
                    c->dirty = 1;
                }
            }
        }
    } else if (k == '\t' || k == '\n' || k == '\r') {
        c->chat_mode = 1; c->chat_input[0] = '\0';
    } else if (k == 'q' || k == 3) {
        /* Signal server: lost + leaving */
        if (c->alive) send_msg(c, "playerlost %d", c->slot);
        send_msg(c, "leaveLobby");
        /* Flush: give TCP a chance to deliver before we clear state */
        { struct timespec ts={0,50000000L}; nanosleep(&ts,NULL); }
        c->state = CLI_LOBBY_SEL;
        c->room_id=0; c->slot=0; c->is_host=0;
        c->is_spectator=0; c->alive=0; c->winner[0]='\0';
        c->room_count=0; c->room_cursor=0;
        memset(c->opp_nicks,0,sizeof(c->opp_nicks));
        memset(c->chat,0,sizeof(c->chat)); c->chat_count=0;
        /* Server sends lobby list after leaveLobby; also request it explicitly */
        send_msg(c,"listlobbies");
    }
    c->dirty = 1;
}

static void handle_input(CliCtx *c) {
    char buf[16]; memset(buf,0,sizeof(buf));
    int n = read_key(buf, sizeof(buf));
    if (n <= 0) return;
    switch (c->state) {
        case CLI_LOBBY_SEL:  handle_input_lobby(c,buf,n);   break;
        case CLI_IN_LOBBY:   handle_input_room(c,buf,n);    break;
        case CLI_GAMEOVER:   handle_input_playing(c,buf,n); break; /* arrows=watch */
        case CLI_PLAYING:
            if(!c->paused) handle_input_playing(c,buf,n);
            break;
        default: if (buf[0]=='q'||buf[0]==3) c->running=0; break;
    }
}

/* ============================================================
 * Signal handler
 * ============================================================ */
static volatile int g_sigwinch = 0;
#ifdef SIGWINCH
static void on_sigwinch(int s) { (void)s; g_sigwinch=1; }
#endif
static void on_sigint(int s) {
    /* Reset crash signals to default to prevent re-entry */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGBUS,  SIG_DFL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    /* Restore terminal to sane state without clearing screen */
    if (g_term_raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        term_write("\033[0m", 4);   /* reset colours */
        term_write("\033[?25h", 6); /* show cursor */
        term_write("\r\n", 2);      /* newline */
        g_term_raw_active = 0;
    }
    if (s == SIGINT || s == SIGTERM) { term_write("Interrupted.\n", 13); }
    else { term_write("Crashed — terminal restored.\n", 28); }
    _exit(s == SIGINT || s == SIGTERM ? 0 : 1);
}

/* ============================================================
 * Main
 * ============================================================ */

/* ============================================================
 * Server profile store — same INI format as lineclashd-tool-secret
 *
 * File format (~/.lineclash-cli-servers or --cfg <path>):
 *
 *   [myserver]
 *   host = myserver.example.com
 *   port = 31457
 *   nick = Styrbjorn
 *   hash = <sha256hex of password>
 *
 * Commands:
 *   --list              List saved servers
 *   --add --alias <a> --host <h> [--port <p>] --nick <n> --password <pw>
 *   --remove --alias <a>
 *   --cfg <path>        Use custom config file
 *   <alias>             Connect using saved profile
 * ============================================================ */
static const char *cli_cfg_path(void) {
    if (g_cfg_path_override[0]) return g_cfg_path_override;
    static char path[512];
    const char *home = getenv("HOME");
    snprintf(path, sizeof(path), "%s/.lineclash-cli-servers", home ? home : ".");
    return path;
}

static void cli_profiles_load(void) {
    g_nprofiles = 0;
    FILE *f = fopen(cli_cfg_path(), "r"); if (!f) return;
    char line[512]; CliProfile *cur = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line,'\n'); if(nl)*nl='\0';
        char *cr = strchr(line,'\r'); if(cr)*cr='\0';
        if (line[0] == '[') {
            if (g_nprofiles < CLI_PROFILES_MAX) {
                cur = &g_profiles[g_nprofiles++];
                memset(cur, 0, sizeof(*cur)); cur->port = DEFAULT_PORT;
                size_t len = strlen(line);
                if (len > 1 && line[len-1] == ']') line[len-1] = '\0';
                snprintf(cur->alias, sizeof(cur->alias), "%.63s", line+1);
            }
        } else if (cur) {
            char key[64], val[256]; val[0] = '\0';
            if (sscanf(line, " %63[^=] = %255[^\n]", key, val) >= 1) {
                int kl = (int)strlen(key);
                while (kl > 0 && key[kl-1] == ' ') key[--kl] = '\0';
                if      (!strcmp(key,"host")) snprintf(cur->host, sizeof(cur->host), "%.255s", val);
                else if (!strcmp(key,"port")) cur->port = atoi(val);
                else if (!strcmp(key,"nick")) snprintf(cur->nick, MAX_NICK_LEN, "%.31s", val);
                else if (!strcmp(key,"hash")) snprintf(cur->hash, 65, "%.64s", val);
            }
        }
    }
    fclose(f);
}

static int cli_profiles_save(void) {
    FILE *f = fopen(cli_cfg_path(), "w");
    if (!f) { fprintf(stderr, "Cannot write: %s\n", cli_cfg_path()); return 0; }
    for (int i = 0; i < g_nprofiles; i++) {
        CliProfile *p = &g_profiles[i]; if (!p->alias[0]) continue;
        fprintf(f, "[%s]\nhost = %s\nport = %d\nnick = %s\n",
                p->alias, p->host, p->port, p->nick);
        if (p->hash[0]) fprintf(f, "hash = %s\n", p->hash);
        fprintf(f, "\n");
    }
    fclose(f); return 1;
}

static CliProfile *cli_profile_find(const char *alias) {
    for (int i = 0; i < g_nprofiles; i++)
        if (!strcasecmp(g_profiles[i].alias, alias)) return &g_profiles[i];
    return NULL;
}

static void cli_print_usage(const char *prog) {
    printf("LineClash CLI v%s\n", TETRINETZ_VERSION);
    printf("\nUsage:\n");
    printf("  %s --add --host <host> [--port <port>] --username <user> --password <pw>\n", prog);
    printf("                              Add a server profile (password stored as hash)\n");
    printf("  %s --list                   List all saved server profiles\n", prog);
    printf("  %s --show <alias>           Show details of a saved profile\n", prog);
    printf("  %s --remove <alias>         Remove a saved server profile\n", prog);
    printf("  %s --rename <alias> <new>   Rename a saved server profile\n", prog);
    printf("  %s --connect <alias>        Connect using saved profile\n", prog);
    printf("  %s <alias>                  Connect using saved profile (shorthand)\n", prog);
    printf("  %s <host> [port] [nick] [pass]  Direct connection\n", prog);
    printf("\nOptions:\n");
    printf("  --cfg <path>                Use a custom credentials file\n");
    printf("  --debug                     Show detailed connection trace\n");
    printf("  --version                   Show version\n");
    printf("  --help                      Show this help\n");
    printf("\nCredentials file: %s\n", cli_cfg_path());
}


/* ---- Blocking connect+auth probe for --add --------------------------------
 * Connects (using net_connect with log suppressed), authenticates,
 * waits for authok/autherr, captures server_name.
 * Returns 1 on success, 0 on failure. */
static int g_probe_debug = 0;

static int cli_probe_auth(const char *host, int port, const char *nick,
                          const char *password, char *server_name_out, int sno_sz) {
    /* Store hash in static buffer — keeps it off the stack so recv_buf
     * writes cannot corrupt it via stack layout overlap. */
    static char pass_hash[65];
    sha256_hex(password, pass_hash);
    if (g_probe_debug) fprintf(stderr, "[probe] computed hash len=%zu first8=%.8s\n", strlen(pass_hash), pass_hash);
    /* Suppress log_info stdout output during entire probe */
    log_set_quiet(1);
    net_init();
    sock_t s = net_connect(host, port);

    if (s == INVALID_SOCK) {
        fprintf(stderr, "Cannot connect to %s:%d\n", host, port);
        net_cleanup(); return 0;
    }

    /* Keep socket non-blocking — recv loop uses select() for timeout */

    server_name_out[0] = '\0';
    char recv_buf[4096]; int recv_len = 0;
    int authed = 0, failed = 0;
    time_t deadline = time(NULL) + 10; /* 10-second overall timeout */

    /* Process messages until authok/autherr or timeout.
     * Use select() per iteration so we handle both blocking and
     * non-blocking sockets correctly. */
    while (!authed && !failed) {
        if (time(NULL) >= deadline) {
            fprintf(stderr, "Connection timed out waiting for server\n");
            break;
        }
        /* Wait up to 50ms for data, then check timeout and loop */
        if (recv_len == 0) {
            fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
            struct timeval stv = {0, 50000};
            int sr = select((int)s+1, &rfds, NULL, NULL, &stv);
            if (sr < 0) { fprintf(stderr, "Connection lost\n"); break; }
            if (sr == 0) continue; /* no data yet, check deadline */
        }
        char line[MAX_MSG_LEN];
        int r = net_recv(s, recv_buf, &recv_len, line, sizeof(line));
        if (r < 0) {
            fprintf(stderr, "Connection lost\n");
            break;
        }
        if (r == 0) continue;
        if (!line[0]) continue;
        if (g_probe_debug) fprintf(stderr, "[probe] recv: %s\n", line);

        char cmd[64], args[4][MAX_MSG_LEN];
        proto_parse(line, cmd, args, 4);

        if (!strcmp(cmd, "noconnecting")) {
            char msg[MAX_MSG_LEN];
            snprintf(msg, sizeof(msg), "authnick %s", nick);
            if (g_probe_debug) fprintf(stderr, "[probe] send: %s\n", msg);
            net_send(s, msg);
        } else if (!strcmp(cmd, "servername")) {
            snprintf(server_name_out, sno_sz, "%.63s", args[0]);
        } else if (!strcmp(cmd, "authchallenge")) {
            char msg[MAX_MSG_LEN], resp[65];
            if (g_probe_debug) fprintf(stderr, "[probe] pass_hash(%.8s..., len=%zu)\n", pass_hash, strlen(pass_hash));
            if (!strcmp(args[0], "new")) {
                snprintf(msg, sizeof(msg), "authresp %s", pass_hash);
            } else {
                hmac_sha256_hex(pass_hash, args[0], resp);
                snprintf(msg, sizeof(msg), "authresp %s", resp);
            }
            if (g_probe_debug) fprintf(stderr, "[probe] send: %s\n", msg);
            net_send(s, msg);
        } else if (!strcmp(cmd, "authok")) {
            authed = 1;
        } else if (!strcmp(cmd, "autherr")) {
            /* Join all args for full error message */
            char errmsg[MAX_MSG_LEN] = "";
            for (int ei=0; ei<4 && args[ei][0]; ei++) {
                size_t _rem = sizeof(errmsg)-strlen(errmsg)-1;
                if (ei && _rem > 0) { errmsg[strlen(errmsg)] = ' '; errmsg[strlen(errmsg)+1] = '\0'; _rem--; }
                if (_rem > 0) snprintf(errmsg+strlen(errmsg), _rem+1, "%.*s", (int)_rem, args[ei]);
            }
            fprintf(stderr, "Authentication failed: %s\n", errmsg);
            failed = 1;
        }
        /* Ignore: lobbylist, lobbylistend, servermotd, etc. */
    }

    sock_close(s);
    net_cleanup();
    return authed;
}


int main(int argc, char **argv) {
    CliCtx _ctx; memset(&_ctx, 0, sizeof(_ctx)); CliCtx *cp = &_ctx;
#define c (*cp)
    c.running  = 1;
    c.dirty    = 1;
    c.state    = CLI_CONNECT;
    c.priv_level = 3;

    /* Capture original termios immediately — before any tcsetattr calls */
    tcgetattr(STDIN_FILENO, &g_orig_termios);

    /* Load saved profiles */
    cli_profiles_load();

    /* ---- Argument parsing — same syntax as lineclashd-tool ---- */
    /* Scan for --cfg first */
    for (int i=1; i<argc-1; i++)
        if (!strcmp(argv[i],"--cfg")) { snprintf(g_cfg_path_override,sizeof(g_cfg_path_override),"%.511s",argv[i+1]); cli_profiles_load(); break; }

    /* Parse flags */
    const char *opt_add_host=NULL, *opt_add_user=NULL, *opt_add_pass=NULL;
    int opt_add_port=DEFAULT_PORT, opt_do_add=0, opt_list=0;
    const char *opt_show=NULL, *opt_remove=NULL;
    const char *opt_rename_old=NULL, *opt_rename_new=NULL;
    const char *opt_connect=NULL;
    int opt_help=0, opt_version=0;

    int i;
    for (i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h"))  opt_help=1;
        else if (!strcmp(argv[i],"--version")||!strcmp(argv[i],"-v")) opt_version=1;
        else if (!strcmp(argv[i],"--cfg") && i+1<argc)              i++; /* consumed above */
        else if (!strcmp(argv[i],"--add"))                          opt_do_add=1;
        else if (!strcmp(argv[i],"--debug")) {
            g_probe_debug=1; g_debug=1;
            g_debug_fp = fopen("/tmp/lineclash-cli-debug.log","w");
            if(!g_debug_fp) g_debug_fp=stderr;
            fprintf(g_debug_fp,"lineclash-cli debug log\n"); fflush(g_debug_fp);
        }
        else if (!strcmp(argv[i],"--host")     && i+1<argc)         opt_add_host=argv[++i];
        else if (!strcmp(argv[i],"--port")     && i+1<argc)         opt_add_port=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--username") && i+1<argc)         opt_add_user=argv[++i];
        else if (!strcmp(argv[i],"--password") && i+1<argc)         opt_add_pass=argv[++i];
        else if (!strcmp(argv[i],"--list"))                         opt_list=1;
        else if (!strcmp(argv[i],"--show")   && i+1<argc)           opt_show=argv[++i];
        else if (!strcmp(argv[i],"--remove") && i+1<argc)           opt_remove=argv[++i];
        else if (!strcmp(argv[i],"--rename") && i+2<argc)         { opt_rename_old=argv[++i]; opt_rename_new=argv[++i]; }
        else if (!strcmp(argv[i],"--connect")&& i+1<argc)           opt_connect=argv[++i];
        else if (argv[i][0]=='-') { fprintf(stderr,"Unknown option: %s\n",argv[i]); opt_help=1; }
        else break; /* first non-flag arg = positional (alias or host) */
    }
    int positional_start = i; /* index of first non-flag argument */

    if (opt_version) { printf("LineClash CLI v%s\n", TETRINETZ_VERSION); return 0; }
    if (opt_help || argc < 2) { cli_print_usage(argv[0]); return (argc<2)?0:1; }

    /* ---- --add: verify credentials then save with server_name as alias ---- */
    if (opt_do_add) {
        if (!opt_add_host || !opt_add_user || !opt_add_pass) {
            fprintf(stderr,"--add requires --host, --username, and --password\n"); return 1;
        }
        char srvname[64] = "";
        printf("Connecting to %s:%d...\n", opt_add_host, opt_add_port);
        if (!cli_probe_auth(opt_add_host, opt_add_port, opt_add_user, opt_add_pass, srvname, sizeof(srvname)))
            return 1; /* error already printed */
        /* Use server_name as alias; fall back to host if server sent no name */
        char spaces_fixed[64];
        snprintf(spaces_fixed, sizeof(spaces_fixed), "%.63s", srvname[0] ? srvname : opt_add_host);
        for (int ai=0; spaces_fixed[ai]; ai++) if (spaces_fixed[ai]==' ') spaces_fixed[ai]='_';
        const char *alias = spaces_fixed;
        /* Remove old entry with same host if it exists under a different alias */
        CliProfile *p = cli_profile_find(alias);
        if (!p) {
            if (g_nprofiles >= CLI_PROFILES_MAX) { fprintf(stderr,"Profile limit reached\n"); return 1; }
            p = &g_profiles[g_nprofiles++]; memset(p,0,sizeof(*p));
        }
        char hash[65]; sha256_hex(opt_add_pass, hash);
        snprintf(p->alias, sizeof(p->alias), "%.63s", alias);
        snprintf(p->host,  sizeof(p->host),  "%.255s", opt_add_host);
        p->port = opt_add_port;
        snprintf(p->nick, MAX_NICK_LEN, "%.31s", opt_add_user);
        snprintf(p->hash, 65, "%.64s", hash);
        if (cli_profiles_save())
            printf("Saved profile '%s' in %s\n", alias, cli_cfg_path());
        return 0;
    }

    /* ---- --list ---- */
    if (opt_list) {
        if (!g_nprofiles) { printf("No saved profiles. Use --add to add one.\n"); return 0; }
        printf("%-20s %-32s %-6s %s\n","Alias","Host","Port","Nick");
        printf("%-20s %-32s %-6s %s\n","-----","----","----","----");
        for (int li=0; li<g_nprofiles; li++) {
            CliProfile *p=&g_profiles[li];
            printf("%-20s %-32s %-6d %s\n", p->alias, p->host, p->port, p->nick);
        }
        return 0;
    }

    /* ---- --show ---- */
    if (opt_show) {
        CliProfile *p = cli_profile_find(opt_show);
        if (!p) { fprintf(stderr,"Profile '%s' not found.\n",opt_show); return 1; }
        printf("Alias : %s\n", p->alias);
        printf("Host  : %s\n", p->host);
        printf("Port  : %d\n", p->port);
        printf("Nick  : %s\n", p->nick);
        printf("Hash  : %s\n", p->hash[0] ? p->hash : "(not set)");
        return 0;
    }

    /* ---- --remove ---- */
    if (opt_remove) {
        int found=0;
        for (int ri=0; ri<g_nprofiles; ri++) {
            if (!strcasecmp(g_profiles[ri].alias, opt_remove)) {
                memmove(&g_profiles[ri],&g_profiles[ri+1],(g_nprofiles-ri-1)*sizeof(CliProfile));
                g_nprofiles--; found=1; break;
            }
        }
        if (!found) { fprintf(stderr,"Profile '%s' not found.\n",opt_remove); return 1; }
        if (cli_profiles_save()) printf("Removed '%s'.\n", opt_remove);
        return 0;
    }

    /* ---- --rename ---- */
    if (opt_rename_old) {
        if (cli_profile_find(opt_rename_new)) { fprintf(stderr,"Profile '%s' already exists.\n",opt_rename_new); return 1; }
        CliProfile *p = cli_profile_find(opt_rename_old);
        if (!p) { fprintf(stderr,"Profile '%s' not found.\n",opt_rename_old); return 1; }
        snprintf(p->alias, sizeof(p->alias), "%.63s", opt_rename_new);
        if (cli_profiles_save()) printf("Renamed '%s' to '%s'.\n", opt_rename_old, opt_rename_new);
        return 0;
    }

    /* ---- --connect or positional alias ---- */
    {
        const char *alias = opt_connect ? opt_connect
                          : (positional_start < argc ? argv[positional_start] : NULL);
        if (alias) {
            CliProfile *p = cli_profile_find(alias);
            if (p) {
                snprintf(c.host, 128, "%.127s", p->host);
                c.port = p->port;
                snprintf(c.nick, MAX_NICK_LEN, "%.31s", p->nick);
                snprintf(c.pass_hash, 65, "%.64s", p->hash);
                printf("Connecting as %s to %s:%d...\n", c.nick, c.host, c.port);
            } else if (!opt_connect) {
                /* Not an alias — treat as direct host */
                c.save_on_auth = 1;
                snprintf(c.host, 128, "%.127s", alias);
                int next = positional_start + 1;
                c.port = (next < argc) ? atoi(argv[next++]) : DEFAULT_PORT;
                if (c.port == 0) c.port = DEFAULT_PORT;
                if (next < argc) snprintf(c.nick, MAX_NICK_LEN, "%.31s", argv[next++]);
                else strncpy(c.nick, "Player", MAX_NICK_LEN-1);
                if (next < argc) sha256_hex(argv[next], c.pass_hash);
            } else {
                fprintf(stderr,"Profile '%s' not found. Use --list.\n", alias); return 1;
            }
            goto do_connect;
        }
    }
    {
        /* Interactive prompt — save on auth if manual entry */
        char host[128]="localhost", nick[64]="Player", pw[64]="";
        int port=DEFAULT_PORT; char portbuf[16]="31457";
        printf("LineClash CLI\n");
        if (g_nprofiles > 0) {
            printf("Saved servers: ");
            for (int i=0; i<g_nprofiles && i<5; i++) printf("%s%s",i?", ":"",g_profiles[i].alias);
            if (g_nprofiles > 5) printf(" (+%d more)",g_nprofiles-5);
            printf("\n(Enter alias or leave blank for manual entry)\n");
        }
        printf("Server alias or host [localhost]: "); fflush(stdout);
        if(fgets(host,sizeof(host),stdin)) host[strcspn(host,"\n")]='\0';
        /* Check if input matches a saved alias */
        CliProfile *p = host[0] ? cli_profile_find(host) : NULL;
        if (p) {
            snprintf(c.host,128,"%.127s",p->host); c.port=p->port;
            snprintf(c.nick,MAX_NICK_LEN,"%.31s",p->nick);
            snprintf(c.pass_hash,65,"%.64s",p->hash);
            printf("Using profile [%s]: %s:%d nick=%s\n",p->alias,c.host,c.port,c.nick);
        } else {
            if (!host[0]) strncpy(host,"localhost",127);
            printf("Port [31457]: "); fflush(stdout);
            if(fgets(portbuf,sizeof(portbuf),stdin)) portbuf[strcspn(portbuf,"\n")]='\0';
            if(portbuf[0]) port=atoi(portbuf);
            printf("Nick [Player]: "); fflush(stdout);
            if(fgets(nick,sizeof(nick),stdin)) nick[strcspn(nick,"\n")]='\0';
            if(!nick[0]) strncpy(nick,"Player",63);
            printf("Password: "); fflush(stdout);
            struct termios noecho=g_orig_termios;
            tcgetattr(STDIN_FILENO,&noecho); noecho.c_lflag&=~ECHO;
            tcsetattr(STDIN_FILENO,TCSAFLUSH,&noecho);
            if(fgets(pw,sizeof(pw),stdin)) pw[strcspn(pw,"\n")]='\0';
            tcsetattr(STDIN_FILENO,TCSAFLUSH,&g_orig_termios);
            printf("\n");
            snprintf(c.host,128,"%.127s",host); c.port=port;
            snprintf(c.nick,MAX_NICK_LEN,"%.31s",nick);
            sha256_hex(pw,c.pass_hash);
            c.save_on_auth = 1;  /* save on successful auth */
        }
    }

do_connect:
    /* Connect */
    net_init();
    c.sock = net_connect(c.host, c.port);
    if (c.sock == INVALID_SOCK) {
        /* Restore terminal in case password prompt left it modified */
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        fprintf(stderr, "Cannot connect to %s:%d\n", c.host, c.port);
        return 1;
    }
    net_set_nonblocking(c.sock);

    /* Terminal */
    term_raw();
#ifdef SIGWINCH
    signal(SIGWINCH, on_sigwinch);
#endif
    signal(SIGINT,   on_sigint);
    signal(SIGTERM,  on_sigint);
    signal(SIGPIPE,  SIG_IGN);
    signal(SIGSEGV,  on_sigint);
    signal(SIGABRT,  on_sigint);
    signal(SIGBUS,   on_sigint);
    signal(SIGABRT,  on_sigint); /* restore terminal on abort */
    signal(SIGBUS,   on_sigint); /* restore terminal on bus error */

    /* Main loop */
    while (c.running) {
        /* Handle SIGWINCH */
        if (g_sigwinch) { g_sigwinch=0; c.dirty=1; }

        /* Network */
        net_tick(&c);

        /* Input */
        DLOG("[main] before handle_input state=%d sock=%d\n",c.state,(int)c.sock);
        handle_input(&c);
        DLOG("[main] after handle_input\n");

        /* Gravity tick — before redraw so field is current */
        if(c.state==CLI_PLAYING && !c.is_spectator && c.alive && c.cur.type>=0 && !c.paused) {
            /* Lock delay: piece is grounded */
            if (c.lock_active) {
                Piece test = c.cur; test.y++;
                if (field_valid(&c.field, &test)) {
                    /* Slid off the edge — resume gravity */
                    c.lock_active = 0; c.lock_timer = 0;
                } else {
                    c.lock_timer -= 16; /* ~16ms per tick */
                    if (c.lock_timer <= 0) {
                        c.lock_active = 0;
                        lock_and_spawn(&c);
                        net_send(c.sock, "holdclear");
                    }
                }
            } else {
                if(c.drop_timer > 0) c.drop_timer--;
                if(c.drop_timer == 0) {
                    c.drop_timer = c.drop_interval;
                    if(!piece_move_down(&c.field, &c.cur)) {
                        if (c.lock_delay_ms > 0) {
                            c.lock_active = 1;
                            c.lock_timer  = c.lock_delay_ms;
                        } else {
                            lock_and_spawn(&c);
                            net_send(c.sock, "holdclear");
                        }
                    } else { proto_send_field(c.sock, c.slot, &c.field); c.dirty=1; }
                }
            }
        }

        /* Draw */
        DLOG("[main] before redraw state=%d\n", c.state);
        redraw(&c);
        DLOG("[main] after redraw\n");
        DLOG("[main] before sleep\n");

        /* Sleep 16ms (~60fps) */
        { struct timespec ts = {0, 16000000L}; nanosleep(&ts, NULL); }
    }

    term_restore();
    if (c.sock != INVALID_SOCK) sock_close(c.sock);
    printf("LineClash CLI disconnected.\n");
    return 0;
}
