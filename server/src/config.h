#ifndef CONFIG_H
#define CONFIG_H
/* config.h — LineClash server configuration (server.cfg)
 *
 * Config file format: one "key = value" per line, # comments.
 * Missing keys use defaults listed below.
 */

/* Default paths — overridden at runtime on macOS to use ~/.lineclashd/ */
#if defined(__APPLE__)
#define CFG_FILE          cfg_default_path()
#define CFG_DEFAULT_DB    cfg_default_db()
#define CFG_DEFAULT_LOG   cfg_default_log()
const char *cfg_default_path(void);
const char *cfg_default_db(void);
const char *cfg_default_log(void);
#else
#define CFG_FILE          "/etc/lineclashd.cfg"
#define CFG_DEFAULT_DB    "/var/lib/lineclashd/database.sqlite3"
#define CFG_DEFAULT_LOG   "/var/log/lineclashd.log"
#endif
#define CFG_MAX_MOTD      512
#define CFG_MAX_PATH      256

typedef struct {
    int  port;
    char log_file[CFG_MAX_PATH];
    char db_file[CFG_MAX_PATH];
    char motd[CFG_MAX_MOTD];
    char server_name[128];           /* displayed in client server list */
    int  require_registered;
    int  max_players_per_room;
    int  num_lobbies;
    int  global_taunts;
    int  status_port;    /* HTTP JSON status API port */
    int  status_api;     /* 1=enabled, 0=disabled */
    int  debug_auth;     /* 1=log full hash/nonce/hmac on auth failure */
    int  debug_ingame;   /* 1=log in-game events: specials, attacks, taunts, levels */
    int  allow_bots;     /* 1 = hosts/admins may add AI bots to rooms */
    /* Game defaults — applied to every room at game start */
    int  initial_level;       /* starting level (initiallevel) */
    int  lines_per_level;     /* lines needed per level-up (linesperlevel) */
    int  level_inc;           /* levels added per trigger (levelinc) */
    int  average_levels;      /* 1 = all players share averaged level (averagelevels) */
    int  special_lines;       /* lines needed to earn specials (speciallines) */
    int  special_count;       /* specials awarded per trigger (specialcount) */
    int  special_capacity;    /* max specials held (specialcapacity) */
    /* Speed-up defaults */
    int  speedup_enabled;     /* 1 = speed-up on by default (speedup = on/off) */
    int  speedup_delay;       /* minutes before speed-up starts (speedup_delay) */
    int  speedup_interval;    /* seconds between added lines (speedup_interval) */
    int  speedup_lines;       /* lines added per interval (speedup_lines) */
    int  lock_delay_ms;       /* ms grace before piece locks (0=instant, max 500) */
    int  reconnect_window;    /* seconds a slot is held after disconnect (0=off) */
    int  garbage_system;      /* 0=classic LineClash, 1=guideline (B2B+combo) */
    int  garbage_targeting;   /* 0=all, 1=random single, 2=focused */
    /* Fine-grained garbage tuning (server.cfg only, not per-room) */
    int  btb_bonus;           /* extra lines for Back-to-Back falling blocks (default 1) */
    int  combo_cap;           /* max combo bonus lines (default 4) */
    int  perfect_clear_bonus; /* lines sent on perfect clear (default 10, 0=off) */
    int  garbage_hole_style;
    int  hold_enabled;        /* 1=allow hold piece (default 1) */  /* 0=random col per line, 1=same col per attack, 2=aligned */
} ServerConfig;

/* Load config from path (or CFG_FILE if path is NULL).
 * Missing keys are filled with defaults.
 * Returns 1 on success (even if file not found — defaults applied). */
int  config_load(ServerConfig *cfg, const char *path);

/* Write a default config file if none exists.  Returns 1 on success. */
int  config_write_default(const char *path);
void config_save_motd(const char *motd);
void config_save_key(const char *key, const char *val);   /* update motd= line in config file */

/* Global config — set by config_load, read everywhere */
extern ServerConfig g_cfg;

#endif /* CONFIG_H */
