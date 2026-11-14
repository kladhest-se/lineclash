#ifndef CLIENT_H
#define CLIENT_H

#include "../../shared/include/lineclash.h"

/* ---- Window constants ---- */
#define WIN_W            1060
#define WIN_H            800
#define BLOCK_SIZE       24
#define MINI_BLOCK       10
#define OWN_FIELD_X      12
#define OWN_FIELD_Y      52    /* 28px header + 8px gap + 16px field-header = 52 */
#define SIDEBAR_X        (OWN_FIELD_X + FIELD_WIDTH * BLOCK_SIZE + 14)
#define SIDEBAR_W        178
#define OPP_START_X      (SIDEBAR_X + SIDEBAR_W + 10)
#define OPP_FIELD_W      (MINI_BLOCK * FIELD_WIDTH  + 4)
#define OPP_FIELD_H      (MINI_BLOCK * FIELD_HEIGHT + 4)
#define CHAT_Y           (ATTACK_BAR_Y + ATTACK_BAR_H)  /* flush against attack bar */
#define CHAT_H           128             /* room chat + activity height (was 160, -2 rows) */
#define ATTACK_BAR_H     54              /* specials row (36px) + taunt strip (18px) */
#define ATTACK_BAR_Y     (OWN_FIELD_Y + FIELD_HEIGHT * BLOCK_SIZE + 6)


#define MAX_SAVED_SERVERS  64
#define MAX_ONLINE_PLAYERS 64

typedef struct {
    char nick[MAX_NICK_LEN];
    int  playing;    /* 1=in game, 0=idle */
    int  room_id;    /* lobby id (1+), 0=in lobby-select */
} OnlinePlayer;
#define MAX_SERVER_NAME    64

typedef struct {
    char name[MAX_SERVER_NAME];
    char host[128];
    int  port;
    int favourite, discovered, lan, locally_saved, capacity, ping_ms;
    long long last_used;
    char nick[MAX_NICK_LEN];
    char pw_hash[65];             /* sha256(password) — stored in servers file */
    char password[64];            /* plaintext only in memory, not saved */
    /* Connection status — set after probe/auth test */
    int  online;                  /* 1=reachable, -1=unreachable, 0=unknown */
    int  auth_status;             /* 0=unknown 1=ok -1=auth_fail -2=ver_mismatch -3=unreachable */
    /* Server population — populated after successful lobby list */
    int  rooms_playing;
    int  rooms_total;
    int  users_registered;        /* total registered accounts */
    int  users_playing;           /* currently in a game */
    int  users_idle;              /* connected but not playing */
} SavedServer;

#define MAX_ACT_LINES    12
#define MAX_ACT_LEN      80

typedef struct { char text[MAX_ACT_LEN]; uint32_t colour; int age; } ActLine;

/* ---- Colours ---- */
#define COL_BG           0x1a1a2eff
#define COL_FIELD_BG     0x0d0d1aff
#define COL_BORDER       0x3a3a6aff
#define COL_GRID         0x16162aff
#define COL_PIECE_I      0x00c8ffff
#define COL_PIECE_O      0xffe800ff
#define COL_PIECE_T      0xb400ffff
#define COL_PIECE_S      0x00e800ff
#define COL_PIECE_Z      0xff2020ff
#define COL_PIECE_J      0x0038ffff
#define COL_PIECE_L      0xff8c00ff
#define COL_GARBAGE      0x606060ff
#define COL_SP_A         0xff4040ff
#define COL_SP_C         0x40ff40ff
#define COL_SP_L         0xff40ffff
#define COL_SP_N         0x40ffffff
#define COL_SP_R         0xffff40ff
#define COL_SP_S         0xff8040ff
#define COL_SP_B         0x4040ffff
#define COL_SP_Q         0xff40a0ff
#define COL_SP_G         0xa0a0a0ff
#define COL_WHITE        0xffffffff
#define COL_LTGRAY       0xc0c0c0ff
#define COL_GRAY         0x808080ff
#define COL_DKGRAY       0x404040ff
#define COL_BLACK        0x000000ff
#define COL_HIGHLIGHT    0x2a2a5aff
#define COL_HOSTGOLD     0xffd700ff
#define COL_OPEN_GREEN   0x22cc44ff
#define COL_INGAME_RED   0xcc2222ff
#define COL_FULL_ORANGE  0xdd8800ff

/* ---- States ---- */
typedef enum {
    CS_ACCOUNT,        /* saved central accounts and HTTPS sign-in */
    CS_CONNECT,        /* launch screen: server/nick/options */
    CS_SERVER_LIST,    /* persistent server list screen */
    CS_AUTH,           /* waiting for server auth response */
    CS_LOBBY_SELECT,
    CS_IN_LOBBY,
    CS_PLAYING,
    CS_GAMEOVER
} ClientState;

/* ---- Sound IDs ---- */
typedef enum {
    SFX_MOVE = 0,
    SFX_ROTATE,
    SFX_LAND,
    SFX_LINE_CLEAR,
    SFX_TETRIS,
    SFX_SPECIAL_SEND,
    SFX_SPECIAL_RECV,
    SFX_GAMEOVER,
    SFX_WIN,
    SFX_CHAT,
    SFX_TAUNT,
    SFX_ROUND_START,
    SFX_ROUND_END,
    SFX_COUNT
} SfxId;

/* ---- Button IDs ---- */
typedef enum {
    BTN_NONE = 0,
    /* Connect screen */
    BTN_CONNECT,
    BTN_QUIT,
    BTN_TOGGLE_SOUND,
    BTN_TOGGLE_MUSIC,
    BTN_MUSIC_PLAYER,
    /* Lobby select */
    BTN_LOBBY_JOIN,
    BTN_LOBBY_ROW_0, BTN_LOBBY_ROW_1, BTN_LOBBY_ROW_2, BTN_LOBBY_ROW_3,
    BTN_LOBBY_ROW_4, BTN_LOBBY_ROW_5, BTN_LOBBY_ROW_6, BTN_LOBBY_ROW_7,
    BTN_LOBBY_ROW_8, BTN_LOBBY_ROW_9,
    BTN_BACK_CONNECT,
    /* In-lobby */
    BTN_START_GAME,
    BTN_READY,          /* player marks themselves ready */
    BTN_BACK_TO_LOBBIES,
    BTN_ADD_BOT_EASY,
    BTN_ADD_BOT_MEDIUM,
    BTN_ADD_BOT_HARD,
    BTN_REMOVE_BOT,
    BTN_OPEN_CHAT,
    BTN_TOGGLE_ATTACKS,
    BTN_TOGGLE_ENDLESS,
    BTN_TOGGLE_TAUNTS,
    BTN_TOGGLE_SPEEDUP,
    BTN_GAME_OPTIONS,     /* open game options popup */
    BTN_AI_PLAYERS,       /* open AI players popup */
    BTN_TOGGLE_GARBAGE_SYSTEM,  /* cycle garbage system */
    BTN_TOGGLE_GARBAGE_TARGET,  /* cycle garbage targeting */
    BTN_TOGGLE_HOLD,            /* toggle hold piece on/off */
    BTN_TAUNT,
    BTN_TEAM_POPUP,       /* open team name popup   */
    BTN_TEAM_POPUP_SET,   /* confirm team name       */
    BTN_GLOBAL_CHAT,      /* open global chat input  */
    BTN_MOTD_EDIT,        /* admin open MOTD popup   */
    BTN_MOTD_POPUP_OK,    /* admin confirm MOTD edit */
    BTN_SRV_ADD,          /* open add-server popup   */
    BTN_SRV_ADD_OK,       /* confirm add/edit server */
    BTN_SRV_DEL,          /* delete selected server  */
    BTN_SRV_EDIT,         /* open rename popup       */
    BTN_SRV_RENAME_OK,    /* confirm rename          */
    BTN_SRV_CONNECT,      /* connect to selected     */
    BTN_SRV_ABOUT,        /* show about popup        */
    BTN_CENTRAL_CONNECT,
    BTN_PUBLIC_SERVERS, BTN_SAVED_SERVERS, BTN_LAN_SERVERS,
    BTN_KEYMAP_POPUP,     /* show keymap editor popup */
    BTN_SRV_REFRESH,      /* force-refresh one server status */
    BTN_DISCOVER, BTN_FAVOURITE, BTN_SERVER_FILTER, BTN_SERVER_SORT,
    BTN_SRV_ROW_0,        /* click on server row 0   */
    /* In-game attack bar: select target player */
    BTN_TARGET_1, BTN_TARGET_2, BTN_TARGET_3,
    BTN_TARGET_4, BTN_TARGET_5, BTN_TARGET_6,
    BTN_TARGET_7, BTN_TARGET_8,
    /* Fire the selected special */
    BTN_FIRE_SPECIAL,
    /* In-game */
    BTN_PAUSE,
    BTN_RESUME,
    BTN_END_GAME,
    BTN_SET_TEAM,   /* in-lobby team change */
    BTN_KICK_POPUP_OK,  /* dismiss kick/ban notification popup */
} BtnId;

/* ---- Mouse state ---- */
typedef struct {
    int x, y, clicked, held;
} MouseState;

/* ---- Chat ---- */
#define MAX_CHAT_LINES  40
#define CHAT_INPUT_MAX 240
typedef struct { char text[MAX_CHAT_LEN]; uint32 colour; int age; } ChatLine;
typedef struct { char text[CHAT_INPUT_MAX]; int len; int active; } ChatInput;

/* ---- Leaderboard entry (per-room, in-game) ---- */
typedef struct {
    char nick[MAX_NICK_LEN];
    int  wins;
    int  streak;
} LbEntry;

/* ---- Server-wide individual leaderboard entry ---- */
#define SERVER_LB_MAX 20
typedef struct {
    char nick[MAX_NICK_LEN];
    char team[MAX_NICK_LEN];
    int  wins;
    int  attacks;
} ServerLbEntry;

/* ---- Server-wide team leaderboard entry ---- */
typedef struct {
    char team[MAX_NICK_LEN];
    int  wins;
    int  members;
} ServerTeamLbEntry;

/* ---- Connect screen text field ---- */
typedef struct {
    char   text[128];
    int    len;
    int    focused;
} TextField;

/* ---- Client context ---- */
typedef struct {
    /* Network */
    sock_t sock;
    uint64_t last_received, last_probe;
    NetReconnect reconnect;
    int reconnect_window, reconnect_room, reconnect_round, reconnect_blocked;
    char reconnect_host[256];

    char   recv_buf[RECV_BUF_SIZE];
    int    recv_len;
    int    running;

    /* State */
    ClientState state;

    /* One remembered identity; its session stays in memory. */
    struct {char handle[15],email[255],token[65];time_t expires;} account;
    int account_join_after,account_job;
    sock_t account_join_socket;
    ClientState account_return;
    TextField tf_account_handle,tf_account_email,tf_account_password,tf_account_confirm;
    int account_create_mode;
    char account_message[256];
    /* Connect screen */
    TextField tf_host;
    TextField tf_port;
    TextField tf_nick;
    TextField tf_pass;   /* password (shown as asterisks) */
    char      connect_error[512];
    char      motd[512];           /* server message of the day */
    int       sound_enabled;
    int       saved_music_enabled;
    int       have_saved_music;

    /* Keymap — SDL scancodes, remappable via settings file */
    struct {
        int move_left;    /* default: 80 (Left arrow) */
        int move_right;   /* default: 79 (Right arrow) */
        int rotate_cw;    /* default: 82 (Up arrow) */
        int rotate_ccw;   /* default: 29 (Z) */
        int soft_drop;    /* default: 81 (Down arrow) */
        int hard_drop;    /* default: 44 (Space) */
        int hold;         /* default:  6 (C) */
        int hold2;        /* default: 225 (LShift) */
        int chat;         /* default: 43 (Tab) */
        int pause;        /* default: 62 (F5) */
        int ready;        /* default: 21 (R) */
    } km;

    /* In-lobby team editing */
    TextField tf_team_lobby;  /* editable team field in sidebar */

    /* Auth state — stored across the async auth handshake */
    char      auth_nick[MAX_NICK_LEN];
    char      auth_pass_hash[65]; /* sha256 hex of password, computed client-side */
    char      auth_nonce[65];     /* nonce received from server in authchallenge  */
    int       auth_nick_is_new;   /* 1 = server said nick is new, send pw hash    */
    int       pending_lobby_id;   /* lobby to join after auth */

    /* Identity */
    char nick[MAX_NICK_LEN];
    char team[MAX_NICK_LEN];

    /* Lobby list */
    LobbyInfo lobby_list[MAX_LOBBIES];
    int       lobby_list_count;
    int       lobby_page;          /* current page of room list (0-based, 3 per page) */
    int       lobby_cursor;
    int       got_lobby_list;
    int       lobby_refresh_timer;  /* counts down; requests fresh list at 0 */

    /* In-lobby / game */
    int ui_scale, piece_patterns, high_contrast;
    int authority;
    unsigned input_sequence;
    int slot, room_id, is_host, is_spectator;
    int no_attacks, last_player_wins, no_taunts, speedup_enabled;
    int garbage_system;    /* 0=classic LineClash, 1=guideline (B2B+combo) */
    int garbage_targeting; /* 0=all opponents, 1=random single, 2=focused */
    int pending_garbage;   /* guideline: lines queued, applied on next lock */
    int hold_enabled;      /* 1=hold piece allowed */

    /* Taunt overlay — shown above chat for a few seconds */
    char taunt_text[256];     /* "Nick slaps OtherNick around..." */
    int  taunt_timer;         /* counts down from ~300 */
    int  taunt_cooldown;      /* per-player anti-spam: ticks down to 0 */
    int  ping_ms;             /* current round-trip latency in ms (-1=unknown) */
    int  ping_timer;          /* ticks until next ping send */

    /* Per-room leaderboard (in-game) */
    LbEntry leaderboard[MAX_PLAYERS];
    int     lb_count;
    LbEntry team_leaderboard[MAX_PLAYERS]; /* per-room team wins */
    int     team_lb_count;
    int     round_num;

    /* Server-wide leaderboard (lobby select screen) */
    ServerLbEntry     server_lb[SERVER_LB_MAX];
    int               server_lb_count;
    int               server_lb_scroll;
    OnlinePlayer      online_players[MAX_ONLINE_PLAYERS];
    int               online_player_count;   /* scroll offset for players LB */
    ServerTeamLbEntry server_team_lb[SERVER_LB_MAX];
    int               server_team_lb_count;
    int               server_team_lb_scroll; /* scroll offset for teams LB */

    /* Saved credentials — username + pw-hash for auto-login */
    char saved_nick[MAX_NICK_LEN];      /* remembered username            */
    char saved_pw_hash[65];             /* sha256(pw) stored locally      */
    int  have_saved_creds;              /* 1 = file was loaded ok         */

    /* My field */
    Field my_field;
    Piece cur, next;
    int   drop_timer, drop_interval, paused;
    /* Hold piece */
    Piece hold;          /* held piece (-1 type = empty) */
    int   hold_used;     /* 1 = hold already used this piece */
    /* Lock delay */
    int   lock_delay_ms; /* from server config (default 300) */
    int   lock_timer;    /* counts down in ms when piece is grounded */
    int   lock_active;   /* 1 = piece is on ground, timer running */
    /* Bot slot tracking */
    int   opp_is_bot[MAX_PLAYERS];       /* 1 if that slot is a bot */
    int   opp_bot_diff[MAX_PLAYERS];      /* 0=easy 1=medium 2=hard */
    int   bots_allowed;                   /* server flag: 1 if bots enabled */
    /* Server-supplied piece queue */
    int   srv_pieces[128]; /* piece types received from server */
    int   srv_piece_head;  /* next piece to use */
    int   srv_piece_tail;  /* end of received queue */

    /* Attack bar — target selection */
    int   attack_target;   /* 1-based slot, 0 = none selected */

    /* Spectator view — which player's field to show in the main field area */
    int   spec_watch_slot; /* 1-based slot, 0 = cycle to first alive */

    /* Opponents */
    Field opp_fields[MAX_PLAYERS];
    char  opp_nicks[MAX_PLAYERS][MAX_NICK_LEN];
    int   opp_alive[MAX_PLAYERS];
    int   opp_spectator[MAX_PLAYERS];
    int   opp_ready[MAX_PLAYERS];   /* ready status for each opponent slot */
    int   is_ready;                 /* our own ready state */

    /* Chat (room-local) */
    ChatLine  chat[MAX_CHAT_LINES];
    int       chat_count;
    int       chat_scroll;          /* 0=bottom (newest), +N=scroll up N lines */
    ChatInput chat_input;

    /* Special effect timers (in ticks) */
    int  immunity_timer;    /* > 0 = immune to specials */
    int  darkness_timer;    /* > 0 = field obscured */
    int  confusion_timer;   /* > 0 = controls reversed/scrambled */
    int  mutate_timer;      /* > 0 = awkward pieces */
    ActLine  act_log[MAX_ACT_LINES];
    int      act_count;
    SavedServer server_list[MAX_SAVED_SERVERS];
    int         server_count;
    int         server_cursor;     /* selected entry in the list */
    int server_tab; /* 0 public, 1 saved */
    int server_filter, server_sort;
    uint64_t probe_started;
    char directory_message[128];
    int         srv_page;          /* current page of server list (6 per page) */
    int         server_popup_open; /* 0=none 1=add 2=unused 3=rename */
    int         server_edit_idx;
    int         music_scroll, music_last_index, about_music_page;
    float credits_offset;
    uint32_t credits_tick;
    int credits_paused;
    int         music_player_open;
    int         about_popup_open;
    TextField   tf_srv_rename;     /* popup 3: new alias only */
    /* Add-server test-connect state */
    int         srv_test_state;    /* 0=idle 1=tcp-connecting 2=auth 3=authing 4=ok -1=fail */
    int         srv_test_timeout;  /* tick countdown for TCP connect timeout */
    char        srv_test_msg[128]; /* error or status message */
    sock_t      srv_test_sock;     /* socket for test connection */
    char        srv_test_recv[RECV_BUF_SIZE];
    int         srv_test_recv_len;
    char        srv_test_hash[65]; /* hash used for test auth */
    char        srv_test_sname[MAX_SERVER_NAME]; /* server-name received during test */
    int         srv_test_ticks;    /* timeout counter */
    int         srv_test_conncount;/* conncount received during probe (deferred) */
    /* Server list auto-refresh */
    int         srvlist_probe_idx;  /* which server we're probing right now */
    int         srvlist_probe_timer;/* frames until next probe */
    TextField   tf_srv_name;       /* popup: custom name */
    TextField   tf_srv_host;       /* popup: host */
    TextField   tf_srv_port;       /* popup: port */
    TextField   tf_srv_nick;       /* popup: nick for this server */
    TextField   tf_srv_pass;       /* popup: password (shown as asterisks) */
    int         tf_srv_focus;      /* 0-4 which field is focused */
    TextField tf_team_popup;
    int       team_popup_open;
    int       game_options_popup_open; /* 0=closed 1=open */
    int       ai_players_popup_open;   /* 0=closed 1=open */
    int central_mode,central_supported;
    char central_code[9],central_resume[65],central_profile[16];
    int controller_enabled;
    int         settings_tab; /* display, controls, audio */
    int         keymap_popup_open;       /* 0=closed 1=open */
    int       keymap_capture_idx;      /* -1=none, 0-10=capturing that row */
    int       license_popup_open;      /* 0=closed 1=open */
    int       license_scroll;          /* scroll offset in lines */
    int       motd_popup_open;
    int       sfx_slider_drag;        /* 1 = dragging SFX volume slider */
    int       music_slider_drag;      /* 1 = dragging Music volume slider */
    int       chat_sb_drag;           /* 1 = dragging room chat scrollbar */
    int       chat_sb_drag_y0;        /* mouse y when drag started */
    int       chat_sb_drag_scroll0;   /* scroll value when drag started */
    int       schat_sb_drag;          /* 1 = dragging server chat scrollbar */
    int       schat_sb_drag_y0;
    int       schat_sb_drag_scroll0;
    int       kick_popup_open;        /* 1 = show kick/ban reason popup */
    char      kick_popup_msg[256];    /* reason text from server */
    int       kick_popup_is_ban;      /* 1 = banned (red), 0 = kicked (orange) */
    int       kick_popup_room_return; /* 1 = return to lobby-select on dismiss */
    char      last_kicked_nick[MAX_NICK_LEN]; /* suppress redundant playerleave chat */
    TextField tf_motd;

    /* Global (server-wide) chat */
    ChatLine  schat[MAX_CHAT_LINES];
    int       schat_count;
    int       schat_scroll;
    ChatInput schat_input;

    /* Status */
    char status[MAX_MSG_LEN];
    int  status_timer;

    /* Admin */
    int       is_admin;           /* 1 = server granted admin access */
    int       priv_level;         /* 1=server-admin 2=room-admin 3=normal */
    ChatInput admin_input;        /* admin command input line         */

    /* Winner */
    char winner[MAX_NICK_LEN];
    int  winner_timer;
} ClientCtx;

/* ---- Prototypes ---- */
void client_run_gui(void);            /* entry: shows connect screen, no cmdline args */
void client_on_key(ClientCtx *c, int scancode, int sym, int is_text, const char *txt);
void client_on_button(ClientCtx *c, BtnId btn);
void client_tick(ClientCtx *c);
void client_network_tick(ClientCtx *c);
void client_handle_msg(ClientCtx *c, const char *line);
void client_sha256_hex(const char *input, char out[65]); /* sha256 hex digest */
void client_hmac_sha256_hex(const char pw_hash_hex[65], const char *nonce, char out[65]);

/* render_sdl.c */
int  sdl_init(void);
void sdl_shutdown(void);
const char *sdl_get_res_dir(void); /* resource dir — Contents/Resources/ on macOS bundle */
void sdl_render_server_list(ClientCtx *c);
void sdl_render_connect(ClientCtx *c);
void sdl_render_accounts(ClientCtx *c);
void sdl_render_lobby_select(ClientCtx *c);
void sdl_render_game(ClientCtx *c);
void sdl_text_start(void);  /* ensure SDL text input active (needed on macOS) */
void sdl_text_stop(void);
int  display_id(ClientCtx *c, int slot);
int  sdl_poll_events(ClientCtx *c);

/* sound.c */
int  sound_init(void);
void sound_shutdown(void);
void sound_play(ClientCtx *c, SfxId id);
typedef struct {
    const char *title, *artist, *license, *license_url, *source_url, *file, *attribution, *intro_file;
} MusicTrack;
typedef struct {
    int index, enabled, paused, shuffle, loading, failed, available;
    double position, duration;
} MusicPlayback;
int music_track_count(void);
const MusicTrack *music_track(int index);
void music_snapshot(MusicPlayback *out);
void music_select(int index);
void music_previous(void);
void music_next(void);
void music_toggle_pause(void);
void music_duck(void);
void music_set_shuffle(int enabled);
int music_get_shuffle(void);
void music_seek(double seconds);
void music_start_menu(void);
void music_start_game(void);
int music_engine_init(void);
void music_engine_shutdown(void);
void music_mix(short *samples,int frames);
void music_load(const char *path);
void music_stop(void);
void music_set_enabled(int enabled);
int  music_get_enabled(void);
void sound_set_sfx_volume(float v);
void client_save_settings(ClientCtx *c);  /* save audio prefs to disk */
float sound_get_sfx_volume(void);
void music_set_volume(float v);
float music_get_volume(void);
void music_tick(void);
void music_set_res_prefix(const char *prefix); /* macOS bundle resource dir */

int client_server_order(ClientCtx *c,int visible[MAX_SAVED_SERVERS]);
int directory_busy(void);
int directory_start_lan(char *,int);
int directory_is_lan(void);
int directory_start(const char *url,char *message,int length);
int directory_poll(SavedServer *rows,int capacity,char *message,int length);
int directory_parse(char *body,SavedServer *rows,int capacity);
int directory_url_valid(const char *url,int local);
void directory_shutdown(void);

enum {ACCOUNT_SIGNIN=1,ACCOUNT_CONTINUE,ACCOUNT_LOGOUT,ACCOUNT_BACK,ACCOUNT_CREATE_MODE,ACCOUNT_LOGIN_MODE,ACCOUNT_REGISTER};
void account_erase(void *,size_t);
int account_http_reply(long,char *,int);
int account_https_start(const char *);
int account_https_busy(void);
int account_https_poll(char *,int);
void account_https_shutdown(void);
void client_accounts_load(ClientCtx *);
void client_accounts_open(ClientCtx *,int);
void client_accounts_action(ClientCtx *,int);
void client_accounts_key(ClientCtx *,int,int,const char *);
void client_accounts_tick(ClientCtx *);
void client_accounts_shutdown(ClientCtx *);
int client_account_ready(ClientCtx *);
void client_account_challenge(ClientCtx *,const char *);

#endif /* CLIENT_H */

void sdl_set_ui_options(int scale,int patterns,int contrast);
void client_cycle_ui_scale(ClientCtx *c);
int client_probe_greeting(SavedServer *server,const char *message);
const char *client_server_status(const SavedServer *server);

void sdl_open_account_code(const char *code);
