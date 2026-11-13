/* config.c — LineClash server configuration loader */
#include "config.h"
#include "../../shared/include/lineclash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(__APPLE__)
#include <sys/stat.h>

static void cfg_ensure_dir(const char *path) {
    /* Create ~/.lineclashd/ if it doesn't exist */
    mkdir(path, 0755);  /* ignore error if already exists */
}

const char *cfg_default_path(void) {
    static char buf[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s/.lineclashd/lineclashd.cfg", home);
    cfg_ensure_dir(buf[0]=='/'?(snprintf(buf,sizeof(buf),"%s/.lineclashd",home),buf):"/tmp");
    snprintf(buf, sizeof(buf), "%s/.lineclashd/lineclashd.cfg", home);
    return buf;
}

const char *cfg_default_db(void) {
    static char buf[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s/.lineclashd/database.sqlite3", home);
    return buf;
}

const char *cfg_default_log(void) {
    static char buf[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sizeof(buf), "%s/.lineclashd/lineclashd.log", home);
    return buf;
}
#endif


ServerConfig g_cfg;
static char active_config_path[CFG_MAX_PATH];

static void cfg_defaults(ServerConfig *c) {
    c->port                = DEFAULT_PORT;
    strncpy(c->log_file,   CFG_DEFAULT_LOG,         CFG_MAX_PATH-1);
    strncpy(c->db_file,    CFG_DEFAULT_DB,           CFG_MAX_PATH-1);
    strncpy(c->motd,       "Welcome to LineClash! Type /help for commands.", CFG_MAX_MOTD-1);
    strncpy(c->server_name,"LineClash",              sizeof(c->server_name)-1);
    c->require_registered  = 0;
    c->max_players_per_room= MAX_PLAYERS;
    c->num_lobbies         = 3;
    c->global_taunts       = 1;
    c->status_port         = 31458;
    c->status_api          = 1;
    c->debug_auth          = 0;
    c->allow_bots          = 1; /* enabled by default */
    c->debug_ingame        = 0;
    /* Game defaults */
    c->initial_level       = 1;
    c->lines_per_level     = 2;
    c->level_inc           = 1;
    c->average_levels      = 1;
    c->special_lines       = 1;
    c->special_count       = 1;
    c->special_capacity    = 18;
    /* Speed-up defaults */
    c->speedup_enabled     = 1;   /* on by default */
    c->speedup_delay       = 10;  /* 10 minutes */
    c->speedup_interval    = 60;  /* every 60 seconds */
    c->speedup_lines       = 1;   /* 1 line per interval */
    /* Lock delay and reconnect */
    c->lock_delay_ms       = 300;
    c->reconnect_window    = 15;
    c->garbage_system      = 0;   /* classic LineClash by default */
    c->garbage_targeting   = 0;   /* all opponents */
    c->btb_bonus           = 1;   /* +1 line for back-to-back falling blocks */
    c->combo_cap           = 4;   /* max extra lines from combo */
    c->perfect_clear_bonus = 10;  /* lines sent on perfect clear (0=off) */
    c->garbage_hole_style  = 0;   /* 0=random per line, 1=same per attack, 2=aligned */
    c->hold_enabled        = 1;   /* hold piece allowed by default */
}

/* Trim leading and trailing whitespace in-place */
static char *trim(char *s) {
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int config_load(ServerConfig *cfg, const char *path) {
    FILE *f;
    char line[1024];
    if (!path) path = CFG_FILE;
    snprintf(active_config_path, sizeof(active_config_path), "%s", path);
    cfg_defaults(cfg);

    f = fopen(path, "r");
    if (!f) {
        /* No config file — use defaults, write one out */
        config_write_default(path);
        return 1;
    }

    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *val;
        /* Strip newline */
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        /* Strip comment */
        char *cm = strchr(line, '#'); if (cm) *cm = '\0';
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = trim(line);
        val = trim(eq + 1);
        if (!*key || !*val) continue;

        if (!strcmp(key, "port"))
            cfg->port = atoi(val);
        else if (!strcmp(key, "log_file"))
            strncpy(cfg->log_file, val, CFG_MAX_PATH-1);
        else if (!strcmp(key, "db_file"))
            strncpy(cfg->db_file, val, CFG_MAX_PATH-1);
        else if (!strcmp(key, "server_name"))
            strncpy(cfg->server_name, val, sizeof(cfg->server_name)-1);
        else if (!strcmp(key, "status_port"))
            cfg->status_port = atoi(val);
        else if (!strcmp(key, "status_api"))
            cfg->status_api = (!strcmp(val,"1")||!strcasecmp(val,"on")||!strcasecmp(val,"yes")) ? 1 : 0;
        else if (!strcmp(key, "debug_auth"))
            cfg->debug_auth = (!strcmp(val,"1")||!strcasecmp(val,"on")||!strcasecmp(val,"yes")) ? 1 : 0;
        else if (!strcmp(key, "allow_bots"))
            cfg->allow_bots = (!strcmp(val,"1")||!strcasecmp(val,"on")||!strcasecmp(val,"yes")) ? 1 : 0;
        else if (!strcmp(key, "debug_ingame"))
            cfg->debug_ingame = (!strcmp(val,"1")||!strcasecmp(val,"on")||!strcasecmp(val,"yes")) ? 1 : 0;
        else if (!strcmp(key, "motd"))
            strncpy(cfg->motd, val, CFG_MAX_MOTD-1);
        else if (!strcmp(key, "require_registered"))
            cfg->require_registered = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "max_players_per_room"))
            cfg->max_players_per_room = atoi(val);
        else if (!strcmp(key, "num_lobbies"))
            cfg->num_lobbies = atoi(val);
        else if (!strcmp(key, "global_taunts"))
            cfg->global_taunts = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "initiallevel"))
            cfg->initial_level = atoi(val);
        else if (!strcmp(key, "linesperlevel"))
            cfg->lines_per_level = atoi(val);
        else if (!strcmp(key, "levelinc"))
            cfg->level_inc = atoi(val);
        else if (!strcmp(key, "averagelevels"))
            cfg->average_levels = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "speciallines"))
            cfg->special_lines = atoi(val);
        else if (!strcmp(key, "specialcount"))
            cfg->special_count = atoi(val);
        else if (!strcmp(key, "specialcapacity"))
            cfg->special_capacity = atoi(val);
        else if (!strcmp(key, "speedup") || !strcmp(key, "speedup_enabled"))
            cfg->speedup_enabled = (!strcmp(val,"1")||!strcasecmp(val,"on")||!strcasecmp(val,"yes")) ? 1 : 0;
        else if (!strcmp(key, "speedup_delay"))
            cfg->speedup_delay = atoi(val);
        else if (!strcmp(key, "speedup_interval"))
            cfg->speedup_interval = atoi(val);
        else if (!strcmp(key, "speedup_lines"))
            cfg->speedup_lines = atoi(val);
        else if (!strcmp(key, "lock_delay") || !strcmp(key, "lock_delay_ms"))
            cfg->lock_delay_ms = atoi(val);
        else if (!strcmp(key, "reconnect_window"))
            cfg->reconnect_window = atoi(val);
        else if (!strcmp(key, "garbage_system"))
            cfg->garbage_system = atoi(val);
        else if (!strcmp(key, "garbage_targeting"))
            cfg->garbage_targeting = atoi(val);
        else if (!strcmp(key, "btb_bonus"))
            cfg->btb_bonus = atoi(val);
        else if (!strcmp(key, "combo_cap"))
            cfg->combo_cap = atoi(val);
        else if (!strcmp(key, "perfect_clear_bonus"))
            cfg->perfect_clear_bonus = atoi(val);
        else if (!strcmp(key, "garbage_hole_style"))
            cfg->garbage_hole_style = atoi(val);
        else if (!strcmp(key, "hold_enabled"))
            cfg->hold_enabled = atoi(val);
    }
    fclose(f);

    /* Clamp values */
    if (cfg->status_port < 0 || cfg->status_port > 65535) cfg->status_port = 0;
    if (cfg->port <= 0 || cfg->port > 65535)  cfg->port = DEFAULT_PORT;
    if (cfg->max_players_per_room < 2)         cfg->max_players_per_room = 2;
    if (cfg->max_players_per_room > MAX_PLAYERS) cfg->max_players_per_room = MAX_PLAYERS;
    if (cfg->num_lobbies < 1)                  cfg->num_lobbies = 1;
    if (cfg->num_lobbies > MAX_LOBBIES)        cfg->num_lobbies = MAX_LOBBIES;
    if (cfg->initial_level < 1)               cfg->initial_level = 1;
    if (cfg->initial_level > 100)             cfg->initial_level = 100;
    if (cfg->lines_per_level < 1)            cfg->lines_per_level = 1;
    if (cfg->level_inc < 1)                  cfg->level_inc = 1;
    if (cfg->special_lines < 1)              cfg->special_lines = 1;
    if (cfg->special_count < 0)              cfg->special_count = 0;
    if (cfg->special_capacity < 0)           cfg->special_capacity = 0;
    if (cfg->special_capacity > 64)          cfg->special_capacity = 64;
    if (cfg->speedup_delay < 1)              cfg->speedup_delay = 1;
    if (cfg->speedup_interval < 5)          cfg->speedup_interval = 5;
    if (cfg->speedup_lines < 1)             cfg->speedup_lines = 1;
    if (cfg->speedup_lines > 4)             cfg->speedup_lines = 4;
    if (cfg->lock_delay_ms < 0)             cfg->lock_delay_ms = 0;
    if (cfg->lock_delay_ms > 500)           cfg->lock_delay_ms = 500;
    if (cfg->reconnect_window < 0)          cfg->reconnect_window = 0;
    if (cfg->reconnect_window > 120)        cfg->reconnect_window = 120;
    if (cfg->garbage_system < 0)            cfg->garbage_system = 0;
    if (cfg->garbage_system > 1)            cfg->garbage_system = 1;
    if (cfg->garbage_targeting < 0)         cfg->garbage_targeting = 0;
    if (cfg->garbage_targeting > 2)         cfg->garbage_targeting = 2;
    if (cfg->btb_bonus < 0)                 cfg->btb_bonus = 0;
    if (cfg->btb_bonus > 4)                 cfg->btb_bonus = 4;
    if (cfg->combo_cap < 0)                 cfg->combo_cap = 0;
    if (cfg->combo_cap > 20)                cfg->combo_cap = 20;
    if (cfg->perfect_clear_bonus < 0)       cfg->perfect_clear_bonus = 0;
    if (cfg->garbage_hole_style < 0)        cfg->garbage_hole_style = 0;
    if (cfg->garbage_hole_style > 2)        cfg->garbage_hole_style = 2;
    if (cfg->hold_enabled < 0)              cfg->hold_enabled = 0;
    if (cfg->hold_enabled > 1)              cfg->hold_enabled = 1;
    return 1;
}

int config_write_default(const char *path) {
    FILE *f;
    /* Ensure parent directory exists for system paths */
#if defined(PLATFORM_POSIX)
    if (strncmp(path, "/var/", 5)==0 || strncmp(path, "/etc/", 5)==0) {
        char dir[CFG_MAX_PATH]; strncpy(dir, path, CFG_MAX_PATH-1);
        char *sl = strrchr(dir, '/');
        if (sl && sl != dir) {
            *sl = '\0';
            /* mkdir -p equivalent — ignore errors (may already exist) */
            char cmd[CFG_MAX_PATH+16]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
            { int _r=system(cmd); (void)_r; }
        }
    }
#endif
    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Warning: cannot write default config to %s\n", path);
        return 0;
    }
    fprintf(f,
        "# LineClash Server Configuration\n"
        "# Lines starting with # are comments.\n"
        "\n"
        "# Human-readable server name shown in the client's server list\n"
        "server_name = LineClash\n"
        "\n"
        "# TCP port to listen on\n"
        "port = %d\n"
        "\n"
        "# Log file path\n"
        "log_file = %s\n"
        "\n"
        "# SQLite database file path\n"
        "db_file = %s\n"
        "\n"
        "# Message of the Day — shown to players when they connect\n"
        "motd = Welcome to LineClash! Good luck and have fun.\n"
        "\n"
        "# Set to 1 to only allow players with registered accounts\n"
        "require_registered = 0\n"
        "\n"
        "# Maximum players per room including bots (2-%d)\n"
        "max_players_per_room = %d\n"
        "\n"
        "# Number of rooms (1-%d)\n"
        "num_lobbies = 3\n"
        "\n"
        "# Set to 0 to disable taunts server-wide\n"
        "global_taunts = 1\n"
        "\n"
        "# HTTP JSON status API (curl/PHP query)\n"
        "# status_api = on   — enable/disable the API (on/off, yes/no, 1/0)\n"
        "# status_port       — TCP port for the API endpoint\n"
        "status_api = on\n"
        "status_port = 31458\n"
        "\n"
        "# AI Bots — allow hosts/admins to add AI bots to rooms\n"
        "# To change at runtime: set allow_bots in this file and restart.\n"
        "# Bot count per room is also persisted across restarts.\n"
        "allow_bots = on\n"
        "\n"
        "# Debug — logs auth failures, specials, attacks, level changes\n"
        "# Disable in production. Runtime: server set debug 1|0\n"
        "debug_auth = off\n"
        "debug_ingame = off\n"
        "\n"
        "# Game settings — defaults applied to every room at game start\n"
        "# These are the starting values; room hosts can change them per-room.\n"
        "\n"
        "# Starting level (1-100)\n"
        "initiallevel = 1\n"
        "\n"
        "# Lines cleared needed to trigger a level increase\n"
        "linesperlevel = 2\n"
        "\n"
        "# How many levels are added per trigger\n"
        "levelinc = 1\n"
        "\n"
        "# Average all players' levels (1) or keep levels individual (0)\n"
        "# When 1: each player's real level is averaged with others and\n"
        "# that average becomes everyone's active level.\n"
        "averagelevels = 1\n"
        "\n"
        "# Lines cleared needed to earn a special block\n"
        "speciallines = 1\n"
        "\n"
        "# Number of specials awarded per trigger\n"
        "specialcount = 1\n"
        "\n"
        "# Maximum specials a player can hold\n"
        "specialcapacity = 18\n"
        "\n"
        "# Speed-up: after a delay, add lines to all players periodically\n"
        "# Keeps games from stalling when a skilled player outlasts everyone else.\n"
        "#\n"
        "# speedup = on/off         Enable or disable speed-up globally (default: on)\n"
        "# speedup_delay = <min>    Minutes of play before speed-up activates (default: 10)\n"
        "# speedup_interval = <sec> Seconds between line additions once active (default: 60)\n"
        "# speedup_lines = <n>      Lines added per interval, 1-4 (default: 1)\n"
        "#\n"
        "# Room hosts can also toggle speed-up per room:\n"
        "#   chroom <id> set speedup on|off\n"
        "speedup = on\n"
        "speedup_delay = 10\n"
        "speedup_interval = 60\n"
        "speedup_lines = 1\n"
        "\n"
        "# Lock delay: ms grace period after piece lands before it locks (0=instant, max 500)\n"
        "lock_delay = 300\n"
        "\n"
        "# Reconnect window: seconds a disconnected player's slot is held (0=disabled)\n"
        "reconnect_window = 15\n"
        "\n"
        "# Garbage system: 0=classic LineClash, 1=guideline (back-to-back + combos)\n"
        "garbage_system = 0\n"
        "\n"
        "# Garbage targeting: 0=all opponents, 1=random single, 2=focused (cycle)\n"
        "garbage_targeting = 0\n"
        "\n"
        "# Fine-grained garbage tuning (server.cfg only, not per-room)\n"
        "# Back-to-back bonus lines added on consecutive falling blocks clears (guideline mode)\n"
        "btb_bonus = 1\n"
        "\n"
        "# Maximum extra lines from combo counter (0 = disable combos)\n"
        "combo_cap = 4\n"
        "\n"
        "# Lines sent on perfect clear (0 = disabled)\n"
        "perfect_clear_bonus = 10\n"
        "\n"
        "# Garbage hole column style: 0=random per line, 1=same column per attack, 2=always column 5\n"
        "garbage_hole_style = 0\n"
        "\n"
        "# Allow hold piece (0=disabled, 1=enabled)\n"
        "hold_enabled = 1\n",
        DEFAULT_PORT, CFG_DEFAULT_LOG, CFG_DEFAULT_DB,
        MAX_PLAYERS, MAX_PLAYERS, MAX_LOBBIES);
    fclose(f);
    return 1;
}

/* Update motd= line in config file in-place (or append if missing) */
void config_save_key(const char *key, const char *val) {
    /* Generic in-place save: update "key = ..." line in CFG_FILE, or append. */
    const char *path = active_config_path[0] ? active_config_path : CFG_FILE;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char lines[512][512]; int n=0;
    while (n<511 && fgets(lines[n],512,f)) n++;
    fclose(f);
    int found=0, i, kl=(int)strlen(key);
    for (i=0; i<n; i++) {
        if (strncmp(lines[i],key,kl)==0 && (lines[i][kl]=='='||lines[i][kl]==' ')) {
            snprintf(lines[i],512,"%s = %s\n",key,val);
            found=1; break;
        }
    }
    if (!found && n<511) { snprintf(lines[n],512,"%s = %s\n",key,val); n++; }
    f = fopen(path,"w"); if (!f) return;
    for (i=0; i<n; i++) fputs(lines[i],f);
    fclose(f);
}

void config_save_motd(const char *motd) { config_save_key("motd", motd); }
