#ifndef TETRINETZ_H
#define TETRINETZ_H

/* ============================================================
 * LineClash - Shared Header
 * Platform: Linux, macOS, Windows
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* ---- Platform detection ---- */
#if defined(_WIN32) || defined(_WIN64)
  #ifndef PLATFORM_WINDOWS
  #define PLATFORM_WINDOWS
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #ifdef _MSC_VER
  #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef SOCKET sock_t;
  #define INVALID_SOCK   INVALID_SOCKET
  #define sock_close(s)  net_close(s)
  #define sock_error()   WSAGetLastError()
  #define SLEEP_MS(ms)   Sleep(ms)
#else
  #ifndef PLATFORM_POSIX
  #define PLATFORM_POSIX
  #endif
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <sys/select.h>
  typedef int sock_t;
  #define INVALID_SOCK   (-1)
  #define sock_close(s)  net_close(s)
  #define sock_error()   errno
  #define SLEEP_MS(ms)   usleep((ms)*1000)
#endif

void net_close(sock_t s);
void net_flush_all(void);
typedef struct {
    int active, attempts;
    sock_t candidate;
    uint64_t deadline, due, attempt_deadline;
} NetReconnect;
uint64_t net_now_ms(void);
void net_reconnect_begin(NetReconnect *r, uint64_t now, int seconds);
void net_reconnect_stop(NetReconnect *r);
sock_t net_reconnect_poll(NetReconnect *r, const char *host, int port, uint64_t now);
void net_peer_address(sock_t s, char *out, size_t size, const char *fallback);


/* ---- Constants ---- */
/* Version is injected at compile time via -DTETRINETZ_VERSION=\"...\"
 * set by the Makefile from version.mk. Fallback if built manually. */
#ifndef TETRINETZ_VERSION
#  define TETRINETZ_VERSION "dev"
#endif

/* BUILD_DATE comes from src/buildnum.h, generated each compile. */
#ifdef __has_include
#  if __has_include("buildnum.h")
#    include "buildnum.h"
#  endif
#endif
#ifndef BUILD_DATE
#  define BUILD_DATE __DATE__
#endif
#define DEFAULT_PORT       31457
#define NET_CONNECT_TIMEOUT_MS  3000   /* 3 seconds max for TCP connect() */
#define MAX_PLAYERS        8
#define MAX_LOBBIES        10
#define MAX_NICK_LEN       32
#define MAX_NICK_DISPLAY   15   /* max chars shown / enforced at input */
#define MAX_LOBBY_NAME     64
#define MAX_MSG_LEN        512
#define MAX_CHAT_LEN       256
#define FIELD_WIDTH        12
#define FIELD_HEIGHT       22
#define RECV_BUF_SIZE      16384
#define NUM_PIECES         7
#define NUM_SPECIALS      14

/* Cell values: 0=empty, 1-7=piece types, 8=garbage, 9-22=special blocks */
#define CELL_EMPTY         0
#define CELL_GARBAGE       8

/* Special block types (1-based), chars: a c b r o q g s n i v m d f */
#define SP_ADDLINE         1   /* a - Add garbage line to target         */
#define SP_CLEARLINE       2   /* c - Clear bottom line of target        */
#define SP_CLEARSPECIAL    3   /* b - Turn target's specials into bricks */
#define SP_RANDOMCLEAR     4   /* r - Remove random blocks from target   */
#define SP_BLOCKBOMB       5   /* o - Explode block bombs on field       */
#define SP_QUAKEFIELD      6   /* q - Shift blocks randomly              */
#define SP_GRAVITY         7   /* g - All blocks fall, close gaps        */
#define SP_SWITCHFIELD     8   /* s - Swap boards with target            */
#define SP_NUKEFIELD       9   /* n - Clear entire board                 */
#define SP_IMMUNITY       10   /* i - Protect from specials temporarily  */
#define SP_CLEARCOLUMN    11   /* v - Delete a random column             */
#define SP_MUTATE         12   /* m - Next pieces become awkward         */
#define SP_DARKNESS       13   /* d - Obscure target's field             */
#define SP_CONFUSION      14   /* f - Randomly switch target's controls  */

/* Game states */
#define STATE_MENU         0
#define STATE_PLAYING      2
#define STATE_GAMEOVER     3

/* ---- Basic types ---- */
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

/* ---- Tetromino shape ---- */
typedef struct { int dx; int dy; } Cell2;
typedef struct { Cell2 c[4]; }     PieceShape;

/* ---- Piece ---- */
typedef struct {
    int type;      /* 0-6 */
    int rotation;  /* 0-3 */
    int x, y;
} Piece;

/* ---- Field ---- */
typedef struct {
    uint8 cells[FIELD_HEIGHT][FIELD_WIDTH];
    int   score;
    int   level;
    int   lines;
    int   special_count;
    uint8 specials[64];
    int   special_head;
    int   special_tail;
} Field;

/* ---- Player (server-side) ---- */
typedef struct {
    int    slot;
    char   nick[MAX_NICK_LEN];
    char   team[MAX_NICK_LEN];
    char   ip[48];         /* remote IP for banning */
    int    alive;
    int    connected;
    int    ready;          /* 1 = clicked Ready, reset each round */
    int    is_admin;       /* 1 = admin privileges granted */
    int    priv_level;     /* 1=admin, 2=mgr, 3=normal */
    Field  field;
    sock_t sock;
    char   recv_buf[RECV_BUF_SIZE];
    int    recv_len;
    int    spectator;      /* 1 = joined mid-game, watching only */
    /* Anti-cheat: server-side tracking */
    int    sv_lines;       /* lines cleared, tracked server-side */
    int    sv_real_level;  /* this player's individual earned level (for averaging) */
    int    sv_specials_head; /* server-side specials queue head */
    int    sv_specials_tail; /* server-side specials queue tail */
    uint8  sv_specials[64];  /* server-side specials queue */
    time_t sv_last_field;    /* last field update timestamp (heartbeat) */
    int    sv_cheat_strikes; /* consecutive suspicious actions */
    /* Server-authoritative piece queue */
    int    sv_piece_queue[64]; /* piece types (0-6) pre-generated by server */
    int    sv_piece_head;      /* next piece to send to client */
    int    sv_piece_tail;      /* end of generated queue */
    int    sv_piece_sent;      /* how many pieces sent to client so far */
    Field  sv_field;           /* server's authoritative field copy */
    Piece  sv_cur;             /* server's current piece for client */
    int    sv_field_mismatch;  /* consecutive field mismatches */
    /* Client identification */
    char   client_type[32];    /* "gui", "cli", "tool", "" */
    char   client_platform[32];/* "macos","linux","windows","amiga","" */
    char   client_version[16]; /* version string */
    /* Reconnect window */
    char   reconnect_nick[MAX_NICK_LEN]; /* if set, slot is reserved for reconnect */
    time_t reconnect_expires;            /* 0 = not reserved */
    /* Hold piece (server-side: type index, -1=empty) */
    int    hold_type;
    /* Garbage combo/back-to-back tracking */
    int    sv_combo;       /* consecutive line clear count (resets on 0-clear) */
    int    sv_b2b;         /* 1 = previous clear was falling blocks/T-spin (back-to-back eligible) */    /* piece type in hold, -1 = empty */
    int    hold_used;    /* 1 = hold already used this piece */
    /* Bot / AI player */
    int    is_bot;             /* 1 = server-controlled AI, no real socket */
    int    bot_difficulty;     /* 0=easy 1=medium 2=hard */
    long   bot_next_tick_ms;   /* wall-clock ms until next AI action */
    int    bot_col;            /* chosen column for current piece (-1 = not decided) */
    int    bot_rot;            /* chosen rotation for current piece */
    int    bot_phase;          /* 0=deciding 1=rotating 2=moving 3=dropping 4=locking */
    int    bot_cur_x;          /* current piece x during movement */
    int    bot_cur_rot;        /* current piece rotation during movement */
    int    bot_cur_y;          /* current piece y during drop */
    int    bot_darkness_ticks; /* >0: playing blind (makes more mistakes) */
    int    bot_confusion_ticks;/* >0: reversed/scrambled controls */
} Player;

/* ---- Lobby info (client-side, from server) ---- */
typedef struct {
    int  id;
    char name[MAX_LOBBY_NAME];
    int  player_count;
    int  max_players;
    int  in_game;
    int  paused;
    int  no_attacks;
    int  last_player_wins;
    int  no_taunts;
    int  speedup_enabled;
    int  garbage_system;    /* 0=classic, 1=guideline */
    int  garbage_targeting; /* 0=all, 1=random, 2=focused */
    int  hold_enabled;      /* 1=hold piece allowed */
} LobbyInfo;

/* ---- Lobby (server-side) ---- */
typedef struct {
    int session_round; /* increments for every round, including unranked/solo */
    int     id;
    char    name[MAX_LOBBY_NAME];
    int     max_players;
    int     player_count;
    int     game_state;
    int     host_slot;
    Player  players[MAX_PLAYERS];
    int     starting_level;
    int     lines_per_level;
    int     level_increment;
    int     average_levels;      /* 1 = all players share averaged level */
    int     lines_per_special;
    int     specials_added;
    int     specials_capacity;
    int     piece_frequencies[NUM_PIECES];
    int     special_frequencies[NUM_SPECIALS];
    char    last_winner[MAX_NICK_LEN];
    int     no_attacks;
    int     last_player_wins;
    int     no_taunts;
    int     paused;              /* 1 = lobby is paused */
    /* Speed-up: after speedup_delay minutes, add lines every speedup_interval seconds */
    int     speedup_enabled;     /* 1 = speed-up active for this room */
    /* Garbage / attack system */
    int     garbage_system;      /* 0=classic LineClash, 1=guideline (B2B+combo) */
    int     garbage_targeting;   /* 0=all opponents, 1=random single, 2=focused */
    int     hold_enabled;        /* 1=allow hold piece */
    int     speedup_delay;       /* minutes before speed-up kicks in */
    int     speedup_interval;    /* seconds between added lines once active */
    int     speedup_lines;       /* lines added per interval */
    time_t  game_start_time;     /* wall-clock time when the current game started */
    time_t  speedup_last_add;    /* wall-clock time of last speed-up line add */
    char    topic[256];          /* room topic — persisted in DB */
    /* Per-room player leaderboard — reset when room empties */
    char    lb_nicks[MAX_PLAYERS][MAX_NICK_LEN];
    int     lb_wins[MAX_PLAYERS];
    int     lb_count;
    int     round_num;   /* increments each game */
    int     lock_delay_ms; /* ms grace period before piece locks (0=instant) */
    /* Per-round win streaks */
    int     lb_streaks[MAX_PLAYERS]; /* consecutive wins per player */
    int     track_stats; /* 1 if this game should count toward DB stats */
    /* Per-room team leaderboard — reset when room empties */
    char    lb_team_nicks[MAX_PLAYERS][MAX_NICK_LEN];
    int     lb_team_wins[MAX_PLAYERS];
    int     lb_team_count;
} Lobby;

/* ---- Server ---- */
typedef struct {
    sock_t listen_sock;
    Lobby  lobbies[MAX_LOBBIES];
    int    lobby_count;
    int    running;
    int    shutdown_ticks;       /* >0 = countdown; 0 = no pending shutdown */
    int    shutdown_warned[4];   /* flags: [0]=5min [1]=2min [2]=1min [3]=30s warned */
    char   shutdown_reason[256];
} Server;

/* ---- Piece data (defined in field.c) ---- */
extern const PieceShape PIECES[NUM_PIECES][4];
extern const char       PIECE_CHARS[NUM_PIECES];
extern const char       SPECIAL_CHARS[NUM_SPECIALS + 1];

/* ============================================================
 * Shared function prototypes
 * ============================================================ */

/* field.c */
void  field_init(Field *f);
int   field_valid(Field *f, Piece *p);
void  field_lock(Field *f, Piece *p);
int   field_clear_lines(Field *f);
void  field_add_garbage(Field *f, int lines);
void  field_add_garbage_styled(Field *f, int lines, int hole_style);
void  field_nuke(Field *f);
void  field_random_clear(Field *f);
void  field_quake(Field *f);
void  field_gravity(Field *f);
void  field_clear_specials(Field *f);
void  field_block_bomb(Field *f, Piece *p);
void  field_clear_line_bottom(Field *f);
void  field_clear_column(Field *f);
void  field_darkness(Field *f);
void  field_encode(Field *f, char *out, int maxlen);
void  field_decode(Field *f, const char *data);
void  field_add_special(Field *f, uint8 type);
uint8 field_pop_special(Field *f);

/* piece.c */
void piece_random(Piece *p, int *frequencies);
void piece_init(Piece *p, int type);
int  piece_rotate_cw(Field *f, Piece *p);
int  piece_rotate_ccw(Field *f, Piece *p);
int  piece_move_left(Field *f, Piece *p);
int  piece_move_right(Field *f, Piece *p);
int  piece_move_down(Field *f, Piece *p);
void piece_drop(Field *f, Piece *p);
int  piece_spawn(Field *f, Piece *p);

/* net.c */
int    net_init(void);
void   net_cleanup(void);
sock_t net_listen(int port);
sock_t net_connect(const char *host, int port);
sock_t net_connect_nb(const char *host, int port);     /* non-blocking start */
int    net_connect_nb_poll(sock_t s);                  /* poll: 1=ok 0=wait -1=fail */
int    net_send(sock_t s, const char *msg);
int    net_recv(sock_t s, char *buf, int *len, char *line, int maxline);
void   net_set_nonblocking(sock_t s);

/* protocol.c */
void proto_send_join(sock_t s, const char *nick, const char *team, int room_id);
void proto_send_field(sock_t s, int slot, Field *f);
void proto_send_special(sock_t s, int from_slot, uint8 sp, int to_slot);
void proto_send_level(sock_t s, int slot, int level);
void proto_send_gameover(sock_t s, int slot);
int  proto_parse(const char *line, char *cmd, char args[][MAX_MSG_LEN], int max_args);

/* util */
void log_info(const char *fmt, ...);
extern void (*g_log_stat_hook)(void); /* optionally set to count log lines */
void log_error(const char *fmt, ...);
void log_set_file(FILE *f);
void log_set_quiet(int q);   /* suppress log_info stdout — use in tool with --quiet */   /* tee log output to this file in addition to stdout */
int  str_split(const char *s, char delim, char parts[][MAX_MSG_LEN], int max);

#endif /* TETRINETZ_H */
