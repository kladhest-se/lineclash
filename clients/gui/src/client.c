/* client.c - LineClash client logic */
#include "client.h"
/* Ping uses a simple monotonic tick counter to avoid platform overflow issues */
static unsigned long g_ping_tick = 0; /* incremented each client_tick call */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifndef PLATFORM_WINDOWS
#include <sys/stat.h>
#endif

/* ============================================================
 * Client-side SHA-256 (same public-domain impl as server/auth.c)
 * Used to hash the password before sending it over the network.
 * The plaintext password never leaves the client.
 * ============================================================ */
typedef unsigned char  _u8;
typedef unsigned int   _u32;
typedef unsigned long long _u64;
static const _u32 _K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define _ROTR(x,n)(((x)>>(n))|((x)<<(32-(n))))
#define _CH(x,y,z)(((x)&(y))^(~(x)&(z)))
#define _MAJ(x,y,z)(((x)&(y))^((x)&(z))^((y)&(z)))
#define _S0(x)(_ROTR(x,2)^_ROTR(x,13)^_ROTR(x,22))
#define _S1(x)(_ROTR(x,6)^_ROTR(x,11)^_ROTR(x,25))
#define _G0(x)(_ROTR(x,7)^_ROTR(x,18)^((x)>>3))
#define _G1(x)(_ROTR(x,17)^_ROTR(x,19)^((x)>>10))
typedef struct{_u32 h[8];_u8 buf[64];_u64 total;} _Sha256;
static void _sinit(_Sha256*c){
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;c->total=0;}
static void _scomp(_Sha256*c,const _u8*b){
    _u32 w[64],a,r,g,d,e,f,gv,h,t1,t2;int i;
    for(i=0;i<16;i++) w[i]=((_u32)b[i*4]<<24)|((_u32)b[i*4+1]<<16)|((_u32)b[i*4+2]<<8)|b[i*4+3];
    for(i=16;i<64;i++) w[i]=_G1(w[i-2])+w[i-7]+_G0(w[i-15])+w[i-16];
    a=c->h[0];r=c->h[1];g=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];gv=c->h[6];h=c->h[7];
    for(i=0;i<64;i++){t1=h+_S1(e)+_CH(e,f,gv)+_K[i]+w[i];t2=_S0(a)+_MAJ(a,r,g);
        h=gv;gv=f;f=e;e=d+t1;d=g;g=r;r=a;a=t1+t2;}
    c->h[0]+=a;c->h[1]+=r;c->h[2]+=g;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=gv;c->h[7]+=h;}
static void _supd(_Sha256*c,const _u8*data,size_t len){
    size_t fill=(size_t)(c->total%64);c->total+=(_u64)len;
    if(fill){size_t room=64-fill;if(len<room){memcpy(c->buf+fill,data,len);return;}
        memcpy(c->buf+fill,data,room);_scomp(c,c->buf);data+=room;len-=room;}
    while(len>=64){_scomp(c,data);data+=64;len-=64;}
    if(len)memcpy(c->buf,data,len);}
static void _sfin(_Sha256*c,_u8 d[32]){
    _u8 pad[128];_u64 bits=c->total*8;size_t fill=(size_t)(c->total%64);
    size_t pl=(fill<56)?(56-fill):(120-fill);int i;
    memset(pad,0,sizeof(pad));pad[0]=0x80;_supd(c,pad,pl);
    pad[0]=(_u8)(bits>>56);pad[1]=(_u8)(bits>>48);pad[2]=(_u8)(bits>>40);pad[3]=(_u8)(bits>>32);
    pad[4]=(_u8)(bits>>24);pad[5]=(_u8)(bits>>16);pad[6]=(_u8)(bits>>8);pad[7]=(_u8)bits;
    _supd(c,pad,8);
    for(i=0;i<8;i++){d[i*4]=(_u8)(c->h[i]>>24);d[i*4+1]=(_u8)(c->h[i]>>16);
        d[i*4+2]=(_u8)(c->h[i]>>8);d[i*4+3]=(_u8)c->h[i];}}
void client_sha256_hex(const char *input, char out[65]) {
    _Sha256 ctx; _u8 dig[32]; int i;
    _sinit(&ctx); _supd(&ctx,(const _u8*)input,strlen(input)); _sfin(&ctx,dig);
    for(i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)dig[i]);
    out[64]='\0';
}

/* HMAC-SHA256: pw_hash_hex is the 64-char hex of sha256(password),
   nonce is the challenge string from the server.
   out[65] = hex-encoded HMAC result. */
void client_hmac_sha256_hex(const char pw_hash_hex[65], const char *nonce, char out[65]) {
    _u8 key[32], ipad[64], opad[64], inner[32], final_dig[32];
    _Sha256 ctx;
    int i;
    /* Decode key from hex */
    for (i=0;i<32;i++) {
        unsigned int b = 0;
        sscanf(pw_hash_hex + i*2, "%02x", &b);
        key[i] = (_u8)b;
    }
    for (i=0;i<64;i++) {
        _u8 k = (i<32) ? key[i] : 0;
        ipad[i] = k ^ 0x36;
        opad[i] = k ^ 0x5c;
    }
    /* inner = SHA256(ipad || nonce) */
    _sinit(&ctx);
    _supd(&ctx, ipad, 64);
    _supd(&ctx, (const _u8*)nonce, strlen(nonce));
    _sfin(&ctx, inner);
    /* outer = SHA256(opad || inner) */
    _sinit(&ctx);
    _supd(&ctx, opad, 64);
    _supd(&ctx, inner, 32);
    _sfin(&ctx, final_dig);
    for (i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)final_dig[i]);
    out[64]='\0';
}

/* ============================================================
 * Platform headers for file I/O and directory creation
 * ============================================================ */
#if defined(PLATFORM_WINDOWS)
#  include <shlobj.h>      /* SHGetFolderPathA */
#  include <direct.h>      /* _mkdir            */
#else
#  include <sys/stat.h>    /* chmod             */
#endif

/* ============================================================
 * Persistent server list
 * Credentials (nick + pw_hash) are stored per server entry here.
 * Format: one [server] block per entry, key=value lines.
 *
 *  Linux / macOS : $HOME/.lineclash_servers
 *  Windows       : %APPDATA%\LineClash\servers
 * ============================================================ */
static const char *server_list_path(void) {
    static char path[512];
#if defined(PLATFORM_WINDOWS)
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        const char *env = getenv("APPDATA");
        strncpy(appdata, env ? env : ".", sizeof(appdata)-1);
    }
    snprintf(path, sizeof(path), "%s\\LineClash\\servers", appdata);
#else
    const char *home = getenv("HOME"); if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/.lineclash_servers", home);
#endif
    return path;
}

/* ============================================================
 * Client settings — audio prefs and any future toggles
 *
 *  Linux / macOS : $HOME/.lineclash_settings
 *  Windows       : %APPDATA%\LineClash\settings
 *
 * Format: key=value lines, one per line, # comments ignored.
 * ============================================================ */
static const char *settings_path(void) {
    static char path[512];
#if defined(PLATFORM_WINDOWS)
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        const char *env = getenv("APPDATA");
        strncpy(appdata, env ? env : ".", sizeof(appdata)-1);
    }
    snprintf(path, sizeof(path), "%s\\LineClash\\settings", appdata);
#else
    const char *home = getenv("HOME"); if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/.lineclash_settings", home);
#endif
    return path;
}

static void ensure_appdata_dir(const char *filepath) {
#if defined(PLATFORM_WINDOWS)
    char dir[512];
    char *last;
    strncpy(dir, filepath, sizeof(dir)-1);
    dir[sizeof(dir)-1] = '\0';
    last = strrchr(dir, '\\');
    if (last) { *last = '\0'; CreateDirectoryA(dir, NULL); }
#else
    (void)filepath;
#endif
}

static void settings_save(ClientCtx *c) {
    ensure_appdata_dir(settings_path());
    FILE *f = fopen(settings_path(), "w");
    if (!f) return;
    fprintf(f, "# LineClash client settings\n");
    fprintf(f, "sfx = %d\n",    c->sound_enabled);
    fprintf(f, "music = %d\n",  music_get_enabled());
    fprintf(f, "sfx_vol = %.2f\n",   sound_get_sfx_volume());
    fprintf(f, "music_vol = %.2f\n", music_get_volume());
    fprintf(f, "\n# Keymap (SDL scancodes — see https://wiki.libsdl.org/SDL_Scancode)\n");
    fprintf(f, "key_move_left  = %d\n", c->km.move_left);
    fprintf(f, "key_move_right = %d\n", c->km.move_right);
    fprintf(f, "key_rotate_cw  = %d\n", c->km.rotate_cw);
    fprintf(f, "key_rotate_ccw = %d\n", c->km.rotate_ccw);
    fprintf(f, "key_soft_drop  = %d\n", c->km.soft_drop);
    fprintf(f, "key_hard_drop  = %d\n", c->km.hard_drop);
    fprintf(f, "key_hold       = %d\n", c->km.hold);
    fprintf(f, "key_hold2      = %d\n", c->km.hold2);
    fprintf(f, "key_chat       = %d\n", c->km.chat);
    fprintf(f, "key_pause      = %d\n", c->km.pause);
    fprintf(f, "key_ready      = %d\n", c->km.ready); /* start game: Enter or Space */
    fclose(f);
}

void client_save_settings(ClientCtx *c) { settings_save(c); }

static void settings_load(ClientCtx *c) {
    FILE *f = fopen(settings_path(), "r");
    if (!f) return;  /* no settings file yet — keep defaults */
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        char key[64] = "", val[64] = "";
        if (sscanf(p, "%63[^= \t] = %63s", key, val) < 2) continue;
        if (!strcmp(key, "sfx"))    c->sound_enabled   = atoi(val) ? 1 : 0;
        if (!strcmp(key, "music")) {
            c->saved_music_enabled = atoi(val) ? 1 : 0;
            c->have_saved_music    = 1;
        }
        if (!strcmp(key, "sfx_vol"))   sound_set_sfx_volume((float)atof(val));
        if (!strcmp(key, "music_vol")) music_set_volume((float)atof(val));
        if (!strcmp(key, "key_move_left"))  c->km.move_left  = atoi(val);
        if (!strcmp(key, "key_move_right")) c->km.move_right = atoi(val);
        if (!strcmp(key, "key_rotate_cw"))  c->km.rotate_cw  = atoi(val);
        if (!strcmp(key, "key_rotate_ccw")) c->km.rotate_ccw = atoi(val);
        if (!strcmp(key, "key_soft_drop"))  c->km.soft_drop  = atoi(val);
        if (!strcmp(key, "key_hard_drop"))  c->km.hard_drop  = atoi(val);
        if (!strcmp(key, "key_hold"))       c->km.hold       = atoi(val);
        if (!strcmp(key, "key_hold2"))      c->km.hold2      = atoi(val);
        if (!strcmp(key, "key_chat"))       c->km.chat       = atoi(val);
        if (!strcmp(key, "key_pause"))      c->km.pause      = atoi(val);
        if (!strcmp(key, "key_ready"))      c->km.ready      = atoi(val);
    }
    fclose(f);
}

static void server_list_save(ClientCtx *c) {
    ensure_appdata_dir(server_list_path());
    FILE *f = fopen(server_list_path(), "w"); if (!f) return;
    int i;
    for (i = 0; i < c->server_count; i++) {
        SavedServer *s = &c->server_list[i];
        fprintf(f, "[server]\nname=%s\nhost=%s\nport=%d\nnick=%s\nhash=%s\n\n",
                s->name, s->host, s->port, s->nick,
                s->pw_hash[0] ? s->pw_hash : "");
    }
    fclose(f);
#ifndef PLATFORM_WINDOWS
    chmod(server_list_path(), 0600);
#endif
}

static void server_list_load(ClientCtx *c) {
    FILE *f; char line[256];
    c->server_count = 0;
    f = fopen(server_list_path(), "r"); if (!f) return;
    SavedServer cur; memset(&cur, 0, sizeof(cur)); cur.port = DEFAULT_PORT;
    int in_block = 0;
    while (fgets(line, sizeof(line), f) && c->server_count < MAX_SAVED_SERVERS) {
        char *nl=strchr(line,'\n'); if(nl)*nl='\0';
        char *cr=strchr(line,'\r'); if(cr)*cr='\0';
        if (!strcmp(line, "[server]")) {
            if (in_block && cur.host[0]) {
                if (!cur.name[0]) strncpy(cur.name, cur.host, MAX_SERVER_NAME-1);
                c->server_list[c->server_count++] = cur;
            }
            memset(&cur,0,sizeof(cur)); cur.port=DEFAULT_PORT; in_block=1;
        } else if (in_block) {
            char *eq=strchr(line,'='); if(!eq) continue;
            *eq='\0'; char *k=line, *v=eq+1;
            if(!strcmp(k,"name")) strncpy(cur.name,v,MAX_SERVER_NAME-1);
            else if(!strcmp(k,"host")) strncpy(cur.host,v,127);
            else if(!strcmp(k,"port")) cur.port=atoi(v);
            else if(!strcmp(k,"nick")) strncpy(cur.nick,v,MAX_NICK_LEN-1);
            else if(!strcmp(k,"hash")&&strlen(v)==64) strncpy(cur.pw_hash,v,64);
        }
    }
    if (in_block && cur.host[0]) {
        if (!cur.name[0]) strncpy(cur.name, cur.host, MAX_SERVER_NAME-1);
        c->server_list[c->server_count++] = cur;
    }
    fclose(f);
}

/* ============================================================
 * Helpers
 * ============================================================ */
/* ---- UTF-8 to Latin-1 conversion for display ----
 * Converts multi-byte UTF-8 sequences to Latin-1 byte values so the
 * font renderer (which uses Latin-1 indices) can display them.
 * Only handles the Latin-1 supplement range (U+00C0..U+00FF),
 * which covers all Swedish characters (Å Ä Ö å ä ö etc). */
static void utf8_to_latin1(const char *in, char *out, int outsz) {
    int i=0, o=0;
    while(in[i] && o<outsz-1) {
        unsigned char c=(unsigned char)in[i];
        if(c<0x80) {
            /* Plain ASCII — pass through */
            out[o++]=in[i++];
        } else if(c==0xC3 && (unsigned char)in[i+1]>=0x80 && (unsigned char)in[i+1]<=0xBF) {
            /* U+00C0..U+00FF: two-byte UTF-8 0xC3 0x80..0xBF → Latin-1 0xC0..0xFF */
            out[o++]=(char)(0xC0 | ((unsigned char)in[i+1] - 0x80));
            i+=2;
        } else if(c==0xC2 && (unsigned char)in[i+1]>=0x80 && (unsigned char)in[i+1]<=0xBF) {
            /* U+0080..U+00BF: 0xC2 0x80..0xBF → Latin-1 0x80..0xBF (mostly control) */
            out[o++]=(char)(0x80 | ((unsigned char)in[i+1] - 0x80));
            i+=2;
        } else if(c>=0x80) {
            /* Lone high byte — already Latin-1, pass through as-is.
             * This handles strings that were converted to Latin-1 before sending
             * (e.g. chat input) and are echoed back by the server unchanged. */
            out[o++]=in[i++];
        } else {
            out[o++]=in[i++];
        }
    }
    out[o]='\0';
}

/* ---- Basic emoji/smiley substitution for chat display ----
 * Replaces common text emoticons with readable ASCII art equivalents.
 * Applied to both room chat and global chat before display. */
static void emoji_sub(const char *in, char *out, int outsz) {
    static const struct { const char *from; const char *to; } map[] = {
        {":)",  "(^_^)"}, {":D",  "(^O^)"}, {":-)", "(^_^)"}, {":-D","(^O^)"},
        {":(", "(;_;)"}, {":-(", "(;_;)"}, {":P",  "(^~^)"}, {":-P","(^~^)"},
        {";)",  "(^-^)"}, {";-)", "(^-^)"}, {":O",  "(O_O)"}, {":-O","(O_O)"},
        {":>",  "(>^<)"}, {"<3",  "(<3)"}, {"B)",  "(B-)"}, {":*",  "(*^*)" },
        {NULL, NULL}
    };
    int i=0, o=0;
    while(in[i]&&o<outsz-1){
        int matched=0;
        int k;
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

static void ctx_chat(ClientCtx *c, const char *txt, uint32_t col) {
    int i; char l1[MAX_CHAT_LEN], emo[MAX_CHAT_LEN];
    utf8_to_latin1(txt, l1, sizeof(l1));
    emoji_sub(l1, emo, sizeof(emo));
    if (c->chat_count >= MAX_CHAT_LINES) {
        for (i=0;i<MAX_CHAT_LINES-1;i++) c->chat[i]=c->chat[i+1];
        c->chat_count = MAX_CHAT_LINES-1;
    }
    strncpy(c->chat[c->chat_count].text, emo, MAX_CHAT_LEN-1);
    c->chat[c->chat_count].colour = col;
    c->chat[c->chat_count].age    = 0;
    c->chat_count++;
    /* Auto-scroll to bottom unless user has scrolled up */
    if(c->chat_scroll==0) c->chat_scroll=0; /* already at bottom, stay */
}

static void ctx_activity(ClientCtx *c, const char *txt, uint32_t col) {
    int i;
    if (c->act_count >= MAX_ACT_LINES) {
        for (i=0;i<MAX_ACT_LINES-1;i++) c->act_log[i]=c->act_log[i+1];
        c->act_count = MAX_ACT_LINES-1;
    }
    strncpy(c->act_log[c->act_count].text, txt, MAX_ACT_LEN-1);
    c->act_log[c->act_count].colour = col;
    c->act_log[c->act_count].age    = 0;
    c->act_count++;
}
static void ctx_status(ClientCtx *c, const char *msg) {
    strncpy(c->status, msg, MAX_MSG_LEN-1);
    c->status_timer = 200;
}

static void ctx_reset_lobby(ClientCtx *c) {
    int i;
    /* Preserve is_admin and priv_level — server only sends adminok once per session.
       They are cleared on full disconnect in ctx_full_disconnect(). */
    c->slot = c->is_host = c->is_spectator = 0;
    c->no_attacks = 0; c->last_player_wins = 1; c->no_taunts = 0; c->speedup_enabled = 1;
    c->garbage_system = 0; c->garbage_targeting = 0;
    c->pending_garbage = 0;
    c->hold_enabled = 1;
    c->paused = 0; c->winner[0]='\0'; c->winner_timer=0;
    c->attack_target = 0;
    c->hold.type = -1; c->hold_used = 0;
    c->lock_delay_ms = 300; c->lock_timer = 0; c->lock_active = 0;
    c->game_options_popup_open = 0; c->ai_players_popup_open = 0;
    c->lb_count = c->round_num = 0;
    c->team_lb_count = 0;
    c->is_ready = 0;
    c->team_popup_open=0; c->tf_team_popup.len=0; c->tf_team_popup.text[0]='\0';
    /* kick popup survives reset so player sees it after being moved back */
    if(!c->kick_popup_open){ c->kick_popup_msg[0]='\0'; }
    memset(c->schat,0,sizeof(c->schat)); c->schat_count=0; c->schat_scroll=0;
    /* NOTE: schat (server chat) is NOT cleared here — it persists for the whole
     * session. Only ctx_full_disconnect() wipes it. */
    memset(c->chat,0,sizeof(c->chat));   c->chat_count=0;  c->chat_scroll=0;
    c->schat_input.active=0; c->schat_input.len=0; c->schat_input.text[0]='\0';
    memset(c->act_log,0,sizeof(c->act_log)); c->act_count=0;
    field_init(&c->my_field); c->my_field.level=1;
    memset(c->opp_nicks,    0, sizeof(c->opp_nicks));
    memset(c->opp_alive,    0, sizeof(c->opp_alive));
    memset(c->opp_spectator,0, sizeof(c->opp_spectator));
    memset(c->opp_ready,    0, sizeof(c->opp_ready));
    for (i=0;i<MAX_PLAYERS;i++) field_init(&c->opp_fields[i]);
}

static void ctx_join_lobby(ClientCtx *c, int room_id) {
    char buf[MAX_MSG_LEN];
    ctx_reset_lobby(c);
    snprintf(buf,sizeof(buf),"lineclash_join %s - %d", c->nick, room_id);
    net_send(c->sock, buf);
    c->room_id = room_id;
    /* Stay in CS_LOBBY until server confirms with playernum — avoids
     * going into IN_LOBBY if the room turns out to be full */
}

/* NES falling blocks drop interval in 60fps ticks per row.
 * Level 1=48, 2=43, 3=38, 4=33, 5=28, 6=23, 7=18, 8=13, 9=8, 10+=5 */
static int level_to_interval(int level) {
    static const int table[]={48,43,38,33,28,23,18,13,8,6,5,5,5,4,4,4,3,3,3,2};
    int idx=level-1; if(idx<0)idx=0; if(idx>=20)idx=19;
    return table[idx];
}

static int lock_and_spawn(ClientCtx *c) {
    int cleared;
    int freq[NUM_PIECES]={14,14,14,14,14,14,14};
    field_lock(&c->my_field, &c->cur);
    cleared   = field_clear_lines(&c->my_field);
    if (cleared > 0 && c->garbage_system == 1 && c->pending_garbage > 0) {
        /* Guideline: cleared lines cancel pending garbage (counter-attack) */
        int cancel = (cleared == 4) ? 4 : (cleared == 3) ? 2 : (cleared == 2) ? 1 : 0;
        c->pending_garbage -= cancel;
        if (c->pending_garbage < 0) c->pending_garbage = 0;
    }
    if (cleared > 0) {
        c->my_field.lines += cleared;
        c->my_field.score += cleared*cleared*100*c->my_field.level;
        /* Tell server how many lines were cleared so it awards specials */
        { char lcbuf[32]; snprintf(lcbuf,sizeof(lcbuf),"lineclear %d",cleared);
          net_send(c->sock, lcbuf); }
        /* Level updated via fieldack; specials via addspecial */
        if (cleared==1) sound_play(c, SFX_LINE_CLEAR);
        else            sound_play(c, SFX_TETRIS);
        /* Garbage is now server-authoritative — server handles attacks on lineclear */
    } else {
        sound_play(c, SFX_LAND);
    }
    /* Guideline: flush pending garbage before spawning next piece */
    if (c->pending_garbage > 0) {
        field_add_garbage_styled(&c->my_field, c->pending_garbage, 0);
        c->pending_garbage = 0;
        proto_send_field(c->sock, c->slot, &c->my_field);
    }
    c->cur=c->next;
    /* Use server-supplied next piece if available, else fallback */
    if (c->srv_piece_head < c->srv_piece_tail) {
        int ntype = c->srv_pieces[c->srv_piece_head++ % 128];
        piece_init(&c->next, ntype);
    } else {
        piece_random(&c->next, freq);
    }
    c->hold_used = 0; /* hold resets on new piece */
    if (!piece_spawn(&c->my_field,&c->cur)) {
        c->state=CS_GAMEOVER;
        /* Send playerlost first so server marks us dead immediately,
         * then send final field so others see our topped-out state. */
        proto_send_gameover(c->sock,c->slot);
        proto_send_field(c->sock, c->slot, &c->my_field);
        ctx_chat(c,"You topped out!",COL_INGAME_RED);
        sound_play(c,SFX_GAMEOVER);
        return 0;
    }
    c->drop_timer=c->drop_interval;
    proto_send_field(c->sock,c->slot,&c->my_field);
    return 1;
}

/* Map server slot → local display ID: own = 1, opponents = 2,3,4... by slot order */
int display_id(ClientCtx *c, int slot) {
    if(slot==c->slot) return 1;
    int n=2,i;
    for(i=0;i<MAX_PLAYERS;i++){
        if(i+1==c->slot) continue;
        if(c->opp_nicks[i][0]){
            if(i+1==slot) return n;
            n++;
        }
    }
    return slot; /* fallback */
}

/* ============================================================
 * Message handler
 * ============================================================ */
void client_handle_msg(ClientCtx *c, const char *line) {
    char cmd[MAX_MSG_LEN], args[16][MAX_MSG_LEN];
    int n = proto_parse(line,cmd,args,16);
    if (!n) return;

    /* ---- KICKED / BANNED notification — show popup before disconnect ---- */
    if (!strcmp(cmd,"kicked") && n>=2) {
        /* Reconstruct full reason (may be multi-word) */
        char reason[256]=""; int ri;
        for(ri=0;ri<n-1&&ri<15;ri++){
            if(ri) strncat(reason," ",sizeof(reason)-strlen(reason)-1);
            strncat(reason,args[ri],sizeof(reason)-strlen(reason)-1);
        }
        int is_ban=(strstr(reason,"anned")!=NULL); /* "banned"/"kickbanned" */
        strncpy(c->kick_popup_msg,reason,sizeof(c->kick_popup_msg)-1);
        c->kick_popup_is_ban=is_ban;
        /* Room-kick: set flag so popup dismiss returns player to lobby select */
        c->kick_popup_room_return = (!is_ban &&
            (c->state==CS_IN_LOBBY || c->state==CS_PLAYING || c->state==CS_GAMEOVER));
        c->kick_popup_open=1;
        return;
    }

    if (!strcmp(cmd,"noconnecting")) {
        /* "noconnecting LineClash/1.0" is the server greeting — ignore it.
           Only treat as fatal error if it carries a rejection reason. */
        const char *msg = args[0];
        if (strcmp(msg,"LineClash/1.0") != 0) {
            /* Check if this is an admin-initiated kill — show popup */
            if (strstr(msg, "terminated by an administrator") != NULL) {
                strncpy(c->kick_popup_msg, "Connection terminated by an administrator",
                        sizeof(c->kick_popup_msg)-1);
                c->kick_popup_is_ban = 0;
                c->kick_popup_room_return = 0;
                c->kick_popup_open = 1;
            }
            snprintf(c->connect_error, sizeof(c->connect_error), "%s", msg);
            if (c->sock != INVALID_SOCK) { sock_close(c->sock); c->sock = INVALID_SOCK; }
            c->state = CS_CONNECT;
        }
        return;
    }

    /* ---- AUTH RESPONSES (received while in CS_AUTH state) ---- */
    if (!strcmp(cmd,"authok") && n>=2) {
        int is_new = (strcmp(args[0],"new")==0);
        snprintf(c->connect_error, sizeof(c->connect_error), "%s",
                 is_new ? "Account created!" : "Logged in.");
        c->state = CS_LOBBY_SELECT;
        c->got_lobby_list = 1;
        c->lobby_refresh_timer = 188;
        /* Mark matching server entry as auth-ok */
        {int si2; for(si2=0;si2<c->server_count;si2++)
            if(!strcasecmp(c->server_list[si2].host,c->tf_host.text))
                {c->server_list[si2].auth_status=1;c->server_list[si2].online=1;break;}}
        /* Tell server our client type/platform */
#if defined(__APPLE__)
        net_send(c->sock, "clientinfo gui macos " TETRINETZ_VERSION);
#elif defined(_WIN32)
        net_send(c->sock, "clientinfo gui windows " TETRINETZ_VERSION);
#else
        net_send(c->sock, "clientinfo gui linux " TETRINETZ_VERSION);
#endif
        net_send(c->sock, "reqleaderboard");
        return;
    }
    if (!strcmp(cmd,"autherr") && n>=2) {
        snprintf(c->connect_error, sizeof(c->connect_error), "%s", args[0]);
        {int si2; for(si2=0;si2<c->server_count;si2++)
            if(!strcasecmp(c->server_list[si2].host,c->tf_host.text))
                {c->server_list[si2].auth_status=-1;c->server_list[si2].online=1;break;}}
        if (c->sock != INVALID_SOCK) { sock_close(c->sock); c->sock = INVALID_SOCK; }
        c->is_admin=0; c->priv_level=0;
        c->state = CS_SERVER_LIST;
        return;
    }
    /* ---- Step 2: server issued a challenge ---- */
    if (!strcmp(cmd,"authchallenge") && n>=2) {
        char buf[MAX_MSG_LEN];
        char resp[65];
        if (strcmp(args[0], "new") == 0) {
            /* New account: send sha256(password) directly */
            c->auth_nick_is_new = 1;
            strncpy(resp, c->auth_pass_hash, 65);
        } else {
            /* Existing account: send HMAC-SHA256(key=sha256(pw), data=nonce) */
            c->auth_nick_is_new = 0;
            strncpy(c->auth_nonce, args[0], 65);
            client_hmac_sha256_hex(c->auth_pass_hash, c->auth_nonce, resp);
        }
        snprintf(buf, sizeof(buf), "authresp %s", resp);
        net_send(c->sock, buf);
        return;
    }

    /* Lobby list — format: cmd id active max state no_attacks last_player_wins no_taunts paused speedup name... */
    if (!strcmp(cmd,"lobbyinfo") && n>=9) {
        int id=atoi(args[0]), idx=c->lobby_list_count, i;
        for (i=0;i<c->lobby_list_count;i++) if(c->lobby_list[i].id==id){idx=i;break;}
        if (idx==c->lobby_list_count&&c->lobby_list_count<MAX_LOBBIES) c->lobby_list_count++;
        c->lobby_list[idx].id               = id;
        c->lobby_list[idx].player_count     = atoi(args[1]);
        c->lobby_list[idx].max_players      = atoi(args[2]);
        c->lobby_list[idx].in_game          = (strcmp(args[3],"ingame")==0);
        c->lobby_list[idx].no_attacks       = atoi(args[4]);
        c->lobby_list[idx].last_player_wins = atoi(args[5]);
        c->lobby_list[idx].no_taunts        = atoi(args[6]);
        c->lobby_list[idx].paused           = atoi(args[7]);
        c->lobby_list[idx].speedup_enabled  = (n>9) ? atoi(args[8]) : 1;
        c->lobby_list[idx].garbage_system   = (n>10) ? atoi(args[9])  : 0;
        c->lobby_list[idx].garbage_targeting= (n>11) ? atoi(args[10]) : 0;
        c->lobby_list[idx].hold_enabled     = (n>12) ? atoi(args[11]) : 1;
        /* args[12..] = name */
        { char nm[MAX_LOBBY_NAME]=""; int ni;
          for(ni=12;ni<n&&ni<21;ni++){
              if(ni>12) strncat(nm," ",MAX_LOBBY_NAME-strlen(nm)-1);
              strncat(nm,args[ni],MAX_LOBBY_NAME-strlen(nm)-1);}
          snprintf(c->lobby_list[idx].name,MAX_LOBBY_NAME,"%s",nm); }
        return;
    }
    if (!strcmp(cmd,"lobbylistend")) {
        c->got_lobby_list=1;
        /* Aggregate lobby stats into the matching saved server entry */
        {int si;
         for(si=0;si<c->server_count;si++){
             if(!strcasecmp(c->server_list[si].host,c->tf_host.text)&&
                c->server_list[si].port==(c->tf_port.len>0?atoi(c->tf_port.text):DEFAULT_PORT)){
                 int rp=0,rt=0,playing=0,idle=0,pi2;
                 for(pi2=0;pi2<c->lobby_list_count;pi2++){
                     rt++;
                     if(c->lobby_list[pi2].in_game){
                         rp++;
                         playing+=c->lobby_list[pi2].player_count;
                     } else {
                         idle+=c->lobby_list[pi2].player_count;
                     }
                 }
                 c->server_list[si].rooms_playing=rp;
                 c->server_list[si].rooms_total=rt;
                 c->server_list[si].users_playing=playing;
                 c->server_list[si].users_idle=idle;
                 c->server_list[si].online=1;
                 c->server_list[si].auth_status=1;
                 break;
             }
         }}
        if (c->state == CS_AUTH) {
            char buf[MAX_MSG_LEN];
            snprintf(buf,sizeof(buf),"authnick %s", c->auth_nick);
            net_send(c->sock, buf);
        }
        return;
    }
    if (!strcmp(cmd,"lobbyupdate") && n>=8) {
        int id=atoi(args[0]),i,idx=-1;
        for(i=0;i<c->lobby_list_count;i++) if(c->lobby_list[i].id==id){idx=i;break;}
        if(idx<0&&c->lobby_list_count<MAX_LOBBIES) idx=c->lobby_list_count++;
        if(idx>=0){
            c->lobby_list[idx].id               = id;
            c->lobby_list[idx].player_count     = atoi(args[1]);
            c->lobby_list[idx].max_players      = atoi(args[2]);
            c->lobby_list[idx].in_game          = (strcmp(args[3],"ingame")==0);
            c->lobby_list[idx].no_attacks       = atoi(args[4]);
            c->lobby_list[idx].last_player_wins = atoi(args[5]);
            c->lobby_list[idx].no_taunts        = atoi(args[6]);
            c->lobby_list[idx].paused           = (n>8)?atoi(args[7]):0;
            c->lobby_list[idx].speedup_enabled  = (n>9)?atoi(args[8]):1;
            c->lobby_list[idx].garbage_system   = (n>10)?atoi(args[9]):0;
            c->lobby_list[idx].garbage_targeting= (n>11)?atoi(args[10]):0;
            c->lobby_list[idx].hold_enabled     = (n>12)?atoi(args[11]):1;
            { char nm[MAX_LOBBY_NAME]=""; int ni;
              for(ni=12;ni<n&&ni<21;ni++){
                  if(ni>12) strncat(nm," ",MAX_LOBBY_NAME-strlen(nm)-1);
                  strncat(nm,args[ni],MAX_LOBBY_NAME-strlen(nm)-1);}
              snprintf(c->lobby_list[idx].name,MAX_LOBBY_NAME,"%s",nm); }
        }
        return;
    }

    if (!strcmp(cmd,"servername") && n>=2) {
        int si;
        for (si=0;si<c->server_count;si++){
            if(!strcasecmp(c->server_list[si].host,c->tf_host.text)){
                /* Always update name from server — only keep user alias if explicitly set */
                if(!strcmp(c->server_list[si].name,c->server_list[si].host))
                    strncpy(c->server_list[si].name,args[0],MAX_SERVER_NAME-1);
                break;
            }
        }
        return;
    }
    if (!strcmp(cmd,"usercount") && n>=2) {
        int si;
        for (si=0;si<c->server_count;si++){
            if(!strcasecmp(c->server_list[si].host,c->tf_host.text)){
                c->server_list[si].users_registered=atoi(args[0]);
                break;
            }
        }
        return;
    }
    if (!strcmp(cmd,"conncount") && n>=2) {
        /* Total connected users: idle = conncount - playing */
        int si, connected=atoi(args[0]);
        for (si=0;si<c->server_count;si++){
            if(!strcasecmp(c->server_list[si].host,c->tf_host.text)){
                int playing=c->server_list[si].users_playing;
                c->server_list[si].users_idle = connected > playing
                                                ? connected - playing : 0;
                break;
            }
        }
        return;
    }
    if (!strcmp(cmd,"motd") && n>=2) {
        char motd[512]=""; int mi;
        for(mi=0;mi<n-1&&mi<16;mi++){
            if(mi) strncat(motd," ",sizeof(motd)-strlen(motd)-1);
            strncat(motd,args[mi],sizeof(motd)-strlen(motd)-1);}
        strncpy(c->motd, motd, sizeof(c->motd)-1);
        /* Also show on connect screen */
        snprintf(c->connect_error, sizeof(c->connect_error), "%s", motd);
        return;
    }
    if (!strcmp(cmd,"serverleaderboard") && n>=2) {
        int cnt=atoi(args[0]), i;
        c->server_lb_count=0;
        c->server_lb_scroll=0;
        /* Each entry: nick team wins attacks — 4 tokens per entry */
        for(i=0;i<cnt&&i<SERVER_LB_MAX;i++){
            int base=1+i*4;
            if(base+3>=n) break;
            strncpy(c->server_lb[i].nick,   args[base],   MAX_NICK_LEN-1);
            strncpy(c->server_lb[i].team,   args[base+1], MAX_NICK_LEN-1);
            c->server_lb[i].wins    = atoi(args[base+2]);
            c->server_lb[i].attacks = atoi(args[base+3]);
            c->server_lb_count++;
        }
        return;
    }
    if (!strcmp(cmd,"serverteamleaderboard") && n>=2) {
        int cnt=atoi(args[0]), i;
        c->server_team_lb_count=0;
        c->server_team_lb_scroll=0;   /* reset scroll on fresh data */
        /* Each entry: team wins members — 3 tokens per entry */
        for(i=0;i<cnt&&i<SERVER_LB_MAX;i++){
            int base=1+i*3;
            if(base+2>=n) break;
            strncpy(c->server_team_lb[i].team, args[base], MAX_NICK_LEN-1);
            c->server_team_lb[i].wins    = atoi(args[base+1]);
            c->server_team_lb[i].members = atoi(args[base+2]);
            c->server_team_lb_count++;
        }
        return;
    }
    if (!strcmp(cmd,"playerlist")) {
        int i; c->online_player_count=0;
        /* Format: nick status room_id  (3 tokens per player) */
        for(i=0;i+2<n&&c->online_player_count<MAX_ONLINE_PLAYERS;i+=3){
            strncpy(c->online_players[c->online_player_count].nick,
                    args[i],MAX_NICK_LEN-1);
            c->online_players[c->online_player_count].playing=
                (strcmp(args[i+1],"playing")==0)?1:0;
            c->online_players[c->online_player_count].room_id=atoi(args[i+2]);
            c->online_player_count++;
        }
        /* Ensure own nick is always in the list */
        {int found=0,j;
         const char *me=c->auth_nick[0]?c->auth_nick:c->nick;
         for(j=0;j<c->online_player_count;j++)
             if(!strcasecmp(c->online_players[j].nick,me)){found=1;break;}
         if(!found&&me[0]&&c->online_player_count<MAX_ONLINE_PLAYERS){
             strncpy(c->online_players[c->online_player_count].nick,me,MAX_NICK_LEN-1);
             c->online_players[c->online_player_count].playing=0;
             c->online_players[c->online_player_count].room_id=0;
             c->online_player_count++;
         }}
        return;
    }
    if (!strcmp(cmd,"adminok")) {
        c->is_admin=1; c->priv_level=1;
        /* privlevel message also sent by server — notification shown there */
        return;
    }
    if (!strcmp(cmd,"privlevel") && n>=2) {
        c->priv_level = atoi(args[0]);
        c->is_admin = (c->priv_level == 1);
        if (c->priv_level == 1)
            ctx_chat(c,"** Admin privileges granted **",COL_HOSTGOLD);
        else if (c->priv_level == 2)
            ctx_chat(c,"** Manager privileges granted **",COL_HOSTGOLD);
        return;
    }

    /* schat and gmsg must be above state guard — needed in CS_LOBBY_SELECT */
    if (!strcmp(cmd,"schat") && n>=3) {
        /* Reconstruct full message (nick is args[0], msg is args[1..]) */
        char full_msg[MAX_CHAT_LEN]=""; int mi;
        for(mi=1;mi<n-1&&mi<15;mi++){
            if(mi>1) strncat(full_msg," ",sizeof(full_msg)-strlen(full_msg)-1);
            strncat(full_msg,args[mi],sizeof(full_msg)-strlen(full_msg)-1);
        }
        char buf[MAX_CHAT_LEN], l1[MAX_CHAT_LEN], emo[MAX_CHAT_LEN];
        snprintf(buf,sizeof(buf),"<%.31s> %.*s",args[0],(int)(sizeof(buf)-36),full_msg);
        utf8_to_latin1(buf,l1,sizeof(l1));
        emoji_sub(l1,emo,sizeof(emo));
        if(c->schat_count>=MAX_CHAT_LINES){
            int gi; for(gi=0;gi<MAX_CHAT_LINES-1;gi++) c->schat[gi]=c->schat[gi+1];
            c->schat_count=MAX_CHAT_LINES-1;}
        strncpy(c->schat[c->schat_count].text,emo,MAX_CHAT_LEN-1);
        c->schat[c->schat_count].colour=0xaaffaaff;
        c->schat[c->schat_count].age=0;
        c->schat_count++;
        /* Server chat is lobby-only — do NOT echo into room chat */
        sound_play(c,SFX_CHAT);
        return;
    }
    if (!strcmp(cmd,"gmsg") && n>=2) {
        /* Reconstruct full message from all args (same pattern as schat/pline) */
        char gmsg_full[MAX_CHAT_LEN]=""; int gmi;
        for(gmi=0;gmi<n-1&&gmi<15;gmi++){
            if(gmi>0) strncat(gmsg_full," ",sizeof(gmsg_full)-strlen(gmsg_full)-1);
            strncat(gmsg_full,args[gmi],sizeof(gmsg_full)-strlen(gmsg_full)-1);
        }
        /* Track kicked nick so playerleave doesn't also show "<< X left" */
        {const char *kp=strstr(gmsg_full," was kicked from the room by ");
         if(kp){
             /* gmsg_full starts with "*** <nick> was kicked..." */
             const char *ns=gmsg_full; while(*ns=='*'||*ns==' ') ns++;
             int klen=(int)(kp-ns);
             if(klen>0&&klen<MAX_NICK_LEN){
                 strncpy(c->last_kicked_nick,ns,klen);
                 c->last_kicked_nick[klen]='\0';
             }
         }}
        if(args[0][0]=='*'){
            /* *** prefix → schat (server announcement) panel */
            if(c->schat_count>=MAX_CHAT_LINES){
                int gi; for(gi=0;gi<MAX_CHAT_LINES-1;gi++) c->schat[gi]=c->schat[gi+1];
                c->schat_count=MAX_CHAT_LINES-1;
            }
            strncpy(c->schat[c->schat_count].text,gmsg_full,MAX_CHAT_LEN-1);
            c->schat[c->schat_count].colour=0xddddaaffu;
            c->schat[c->schat_count].age=0;
            c->schat_count++;
        } else {
            ctx_chat(c,gmsg_full,0xddddaaffu);
        }
        return;
    }

    if (!strcmp(cmd,"playernum") && n>=2) {
        char buf[MAX_CHAT_LEN]; c->slot=atoi(args[0]);
        c->attack_target = c->slot; /* default target = self (display key 1) */
        c->state = CS_IN_LOBBY;    /* server confirmed join */
        snprintf(buf,sizeof(buf),"You are player %d in Room %d",c->slot,c->room_id);
        ctx_chat(c,buf,COL_LTGRAY); return;
    }

    if (c->state==CS_LOBBY_SELECT || c->state==CS_AUTH || c->state==CS_SERVER_LIST) return;

    if (!strcmp(cmd,"spectator") && n>=2) {
        int was_spec = c->is_spectator;
        c->is_spectator = atoi(args[0]);
        if (c->is_spectator) {
            ctx_chat(c,"Joined as spectator -- waiting for next game",0xaaaaffff);
            c->spec_watch_slot = 0;
        } else if (was_spec) {
            /* Promoted from spectator — ready to play next round */
            c->spec_watch_slot = 0;
            ctx_chat(c,"You are now a full player -- ready for next game!",COL_OPEN_GREEN);
        }
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
    if (!strcmp(cmd,"host") && n>=2) {
        int hs=atoi(args[0]); c->is_host=(hs==c->slot);
        if (c->is_host) { ctx_chat(c,"You are the host -- click START GAME",COL_HOSTGOLD); ctx_status(c,"HOST"); }
        else { char buf[MAX_CHAT_LEN]; snprintf(buf,sizeof(buf),"Host: slot %d",hs); ctx_chat(c,buf,COL_LTGRAY); }
        return;
    }
    if (!strcmp(cmd,"playerjoin") && n>=3) {
        int slot=atoi(args[0]);
        if (slot>=1&&slot<=MAX_PLAYERS) {
            int is_new=(c->opp_nicks[slot-1][0]=='\0');
            strncpy(c->opp_nicks[slot-1],args[1],MAX_NICK_LEN-1);
            /* In lobby state, alive=0 until game starts */
            if (c->state!=CS_PLAYING) c->opp_alive[slot-1]=0;
            if (is_new) field_init(&c->opp_fields[slot-1]);
        }
        /* Only show join message for genuinely new players, not bot re-announcements */
        if (!c->opp_is_bot[slot-1]) {
            char buf[MAX_CHAT_LEN]; snprintf(buf,sizeof(buf),">> %.31s joined (slot %d)",args[1],slot);
            ctx_chat(c,buf,0x88cc88ff);
        }
        return;
    }
    if (!strcmp(cmd,"playerleave") && n>=2) {
        int slot=atoi(args[0]); char buf[MAX_CHAT_LEN]; buf[0]='\0';
        if (slot>=1&&slot<=MAX_PLAYERS) {
            const char *lnick=c->opp_nicks[slot-1][0]?c->opp_nicks[slot-1]:"?";
            /* Only show "left" if this player wasn't just announced as kicked */
            int was_kicked = (c->last_kicked_nick[0] &&
                              !strcasecmp(c->last_kicked_nick, lnick));
            if (!was_kicked)
                snprintf(buf,sizeof(buf),"<< %s left",lnick);
            c->last_kicked_nick[0]='\0'; /* clear after use */
            c->opp_nicks[slot-1][0]='\0';
            c->opp_alive[slot-1]=c->opp_spectator[slot-1]=c->opp_ready[slot-1]=0;
            c->opp_is_bot[slot-1]=0; c->opp_bot_diff[slot-1]=0;
            field_init(&c->opp_fields[slot-1]);
        } else {
            snprintf(buf,sizeof(buf),"<< Player %d left",slot);
        }
        if (buf[0]) ctx_chat(c,buf,0xcc8888ff);
        return;
    }
    if (!strcmp(cmd,"readystatus") && n>=3) {
        int slot=atoi(args[0]), rdy=atoi(args[1]);
        if (slot==c->slot) c->is_ready=rdy;
        else if (slot>=1&&slot<=MAX_PLAYERS) c->opp_ready[slot-1]=rdy;
        return;
    }
    if (!strcmp(cmd,"team")) return;  /* team updates from other players handled below */
    if (!strcmp(cmd,"yourteam") && n>=2) {
        /* Server telling us our own stored team */
        const char *t = (strcmp(args[0],"-")==0) ? "" : args[0];
        strncpy(c->team, t, MAX_NICK_LEN-1);
        strncpy(c->tf_team_lobby.text, t, sizeof(c->tf_team_lobby.text)-1);
        c->tf_team_lobby.len = (int)strlen(c->tf_team_lobby.text);
        return;
    }
    if (!strcmp(cmd,"leaderboard") && n>=3) {
        int i; c->round_num=atoi(args[0]); c->lb_count=atoi(args[1]);
        if (c->lb_count>MAX_PLAYERS) c->lb_count=MAX_PLAYERS;
        for (i=0;i<c->lb_count&&2+i*3+1<n;i++) {
            strncpy(c->leaderboard[i].nick,args[2+i*3],MAX_NICK_LEN-1);
            c->leaderboard[i].wins  =atoi(args[3+i*3]);
            c->leaderboard[i].streak=(4+i*3<n)?atoi(args[4+i*3]):0;
        }
        return;
    }
    if (!strcmp(cmd,"teamleaderboard") && n>=2) {
        int i; c->team_lb_count=atoi(args[0]);
        if(c->team_lb_count>MAX_PLAYERS) c->team_lb_count=MAX_PLAYERS;
        for(i=0;i<c->team_lb_count&&i*2+2<n;i++) {
            strncpy(c->team_leaderboard[i].nick,args[i*2+1],MAX_NICK_LEN-1);
            c->team_leaderboard[i].wins=atoi(args[i*2+2]);
        }
        return;
    }

    if (!strcmp(cmd,"newgame")) {
        int i, freq[NUM_PIECES]={14,14,14,14,14,14,14};
        if (n>=2) c->my_field.level=atoi(args[0]);
        c->paused=0; c->winner[0]='\0'; c->winner_timer=0;
        c->spec_watch_slot=0;
        c->is_ready=0;
        memset(c->opp_ready,0,sizeof(c->opp_ready));
        field_init(&c->my_field);
        for (i=0;i<MAX_PLAYERS;i++) {
            if (i+1!=c->slot) { field_init(&c->opp_fields[i]); c->opp_alive[i]=(c->opp_nicks[i][0]!='\0'); }
        }
        if (!c->is_spectator) {
            c->state=CS_PLAYING;
            c->srv_piece_head = 0; c->srv_piece_tail = 0;
            /* Server will send nextpiece messages; use first two if already buffered */
            if (c->srv_piece_head < c->srv_piece_tail)
                piece_init(&c->cur, c->srv_pieces[c->srv_piece_head++ % 128]);
            else
                piece_random(&c->cur, freq);
            if (c->srv_piece_head < c->srv_piece_tail)
                piece_init(&c->next, c->srv_pieces[c->srv_piece_head++ % 128]);
            else
                piece_random(&c->next, freq);
            piece_spawn(&c->my_field,&c->cur);
            c->drop_interval=level_to_interval(c->my_field.level);
            if (c->drop_interval<2) c->drop_interval=2;
            ctx_chat(c,"=== GAME STARTED ===",COL_HOSTGOLD);
        } else {
            c->state=CS_PLAYING;
            ctx_chat(c,"=== GAME STARTED (spectating) ===",COL_GRAY);
        }
        sound_play(c,SFX_ROUND_START);
        music_load("lineclash_game.mod");
        return;
    }
    if (!strcmp(cmd,"ingame")) {
        c->state=CS_PLAYING;
        if (c->is_spectator) ctx_chat(c,"Game in progress -- spectating",COL_GRAY);
        sound_play(c,SFX_ROUND_START);
        music_load("lineclash_game.mod");
        return;
    }
    if (!strcmp(cmd,"pause") && n>=2) {
        c->paused=atoi(args[0]);
        ctx_chat(c,c->paused?"** PAUSED **":"** RESUMED **",COL_HOSTGOLD); return;
    }
    if (!strcmp(cmd,"endgame")) {
        c->state=CS_IN_LOBBY; c->is_spectator=0;
        c->is_ready=0;
        c->spec_watch_slot=0; /* reset spectator view */
        memset(c->opp_ready,0,sizeof(c->opp_ready));
        memset(c->opp_alive,0,sizeof(c->opp_alive));
        ctx_chat(c,"=== GAME OVER ===",COL_LTGRAY);
        sound_play(c,SFX_ROUND_END);
        music_load("lineclash_menu.mod");
        return;
    }
    if (!strcmp(cmd,"playerlost") && n>=2) {
        int slot=atoi(args[0]); char buf[MAX_CHAT_LEN];
        if (slot==c->slot) {
            c->state=CS_GAMEOVER; ctx_chat(c,"You were eliminated!",COL_INGAME_RED);
            sound_play(c,SFX_GAMEOVER);
        } else if (slot>=1&&slot<=MAX_PLAYERS) {
            c->opp_alive[slot-1]=0;
            snprintf(buf,sizeof(buf),"*** %s eliminated",c->opp_nicks[slot-1][0]?c->opp_nicks[slot-1]:"?");
            ctx_chat(c,buf,COL_INGAME_RED);
        }
        return;
    }
    if (!strcmp(cmd,"playerwon") && n>=2) {
        char buf[MAX_CHAT_LEN];
        strncpy(c->winner,args[0],MAX_NICK_LEN-1); c->winner_timer=360;
        snprintf(buf,sizeof(buf),"*** %.31s WINS! ***",args[0]);
        ctx_chat(c,buf,COL_HOSTGOLD);
        c->state=CS_IN_LOBBY; c->is_spectator=0;
        /* Reset ready state so host can start a new game */
        c->is_ready=0;
        memset(c->opp_ready,0,sizeof(c->opp_ready));
        sound_play(c,SFX_WIN); return;
    }
    if (!strcmp(cmd,"f") && n>=3) {
        int slot=atoi(args[0]);
        if (slot>=1&&slot<=MAX_PLAYERS&&slot!=c->slot) field_decode(&c->opp_fields[slot-1],args[1]);
        return;
    }
    if (!strcmp(cmd,"nextpiece") && n>=2) {
        /* Server pre-supplies piece types — queue them */
        char pc = args[0][0];
        int ptype = -1;
        for (int pi=0; pi<NUM_PIECES; pi++)
            if (PIECE_CHARS[pi]==pc) { ptype=pi; break; }
        if (ptype >= 0 && c->srv_piece_tail - c->srv_piece_head < 126)
            c->srv_pieces[c->srv_piece_tail++ % 128] = ptype;
        return;
    }
    if (!strcmp(cmd,"holdstate") && n>=3) {
        /* holdstate <slot> <ptype> — another player used hold */
        return; /* opponents hold displayed via their field render — no extra action */
    }
    if (!strcmp(cmd,"botinfo") && n>=3) {
        int sl=atoi(args[0])-1;
        if(sl>=0&&sl<MAX_PLAYERS){
            c->opp_is_bot[sl]=1;
            c->opp_bot_diff[sl] = !strcasecmp(args[1],"hard") ? 2 :
                                   !strcasecmp(args[1],"medium") ? 1 : 0;
        }
        return;
    }
    if (!strcmp(cmd,"botsallowed") && n>=2) {
        c->bots_allowed = atoi(args[0]);
        return;
    }
    if (!strcmp(cmd,"pong") && n>=2) {
        unsigned long sent_tick = (unsigned long)atol(args[0]);
        /* Each tick ~16ms; clamp to avoid stale pongs showing huge values */
        unsigned long delta = g_ping_tick - sent_tick;
        c->ping_ms = (delta < 2000) ? (int)(delta * 16) : -1;
        return;
    }
    if (!strcmp(cmd,"addspecial") && n>=2) {
        /* Server awarded us a special — add to local display queue */
        char sp_char = args[0][0];
        int si; uint8 sp=0;
        for(si=0;SPECIAL_CHARS[si];si++) if(SPECIAL_CHARS[si]==sp_char){sp=(uint8)(si+1);break;}
        if (sp) field_add_special(&c->my_field, sp);
        return;
    }
    if (!strcmp(cmd,"lockdelay") && n>=2) {
        c->lock_delay_ms = atoi(args[0]);
        if (c->lock_delay_ms < 0) c->lock_delay_ms = 0;
        if (c->lock_delay_ms > 500) c->lock_delay_ms = 500;
        return;
    }
    if (!strcmp(cmd,"myfield") && n>=2) {
        /* Server-authoritative field update for own field (used by switch special) */
        field_decode(&c->my_field, args[0]);
        /* Respawn piece on the new field geometry */
        piece_spawn(&c->my_field, &c->cur);
        proto_send_field(c->sock, c->slot, &c->my_field);
        return;
    }
    if (!strcmp(cmd,"addline") && n>=2) {
        /* Speed-up: server adds garbage lines to our field */
        int nlines = atoi(args[0]);
        if (nlines > 0 && nlines <= 4) field_add_garbage(&c->my_field, nlines);
        return;
    }
    if (!strcmp(cmd,"speedup") && n>=2) {
        /* Speed-up notification — bright orange, shown in activity log + chat */
        char su_text[MAX_CHAT_LEN] = "";
        int si2;
        for (si2=0; si2<n; si2++) {
            if (si2>0) strncat(su_text," ",sizeof(su_text)-strlen(su_text)-1);
            strncat(su_text,args[si2],sizeof(su_text)-strlen(su_text)-1);
        }
        /* Bright orange-yellow — stands out from normal gray gmsg */
        ctx_activity(c, su_text, 0xffcc00ff);
        ctx_chat(c, su_text, 0xffcc00ff);
        return;
    }
    if (!strcmp(cmd,"fieldack") && n>=4) {
        /* Server confirms authoritative lines/score/level after piece lock */
        int srv_lines = atoi(args[0]);
        int srv_score = atoi(args[1]);
        int srv_level = atoi(args[2]);
        c->my_field.lines = srv_lines;
        c->my_field.score = srv_score;
        if (srv_level != c->my_field.level) {
            c->my_field.level = srv_level;
            c->drop_interval = level_to_interval(srv_level);
            proto_send_level(c->sock, c->slot, srv_level);
        }
        return;
    }
    if (!strcmp(cmd,"lvl") && n>=3) {
        int slot=atoi(args[0]),level=atoi(args[1]);
        if (slot==c->slot) {
            c->my_field.level=level;
            c->drop_interval=level_to_interval(level);
        }
        return;
    }
    if (!strcmp(cmd,"sb") && n>=4) {
        int to_slot=atoi(args[0]); char sp_char=args[1][0]; int from_slot=atoi(args[2]);
        uint8 sp=0; int si;

        /* Map incoming char to SP_ index */
        for (si=0;SPECIAL_CHARS[si];si++) if(SPECIAL_CHARS[si]==sp_char){sp=(uint8)(si+1);break;}

        /* Apply effect to own field if we are the target */
        if (to_slot==c->slot && !c->is_spectator) {
            if (c->immunity_timer>0 && sp!=SP_IMMUNITY) {
                /* Blocked by immunity — still show activity but no effect */
            } else {
                switch(sp){
                    case SP_ADDLINE:
                        if (c->garbage_system == 1) {
                            /* Guideline: queue garbage, applied on next lock */
                            if (c->pending_garbage < 20) c->pending_garbage++;
                        } else {
                            field_add_garbage(&c->my_field, 1);
                        }
                        break;
                    case SP_CLEARLINE:   field_clear_line_bottom(&c->my_field);      break;
                    case SP_CLEARSPECIAL:field_clear_specials(&c->my_field);         break;
                    case SP_RANDOMCLEAR: field_random_clear(&c->my_field);           break;
                    case SP_BLOCKBOMB:   field_block_bomb(&c->my_field,&c->cur);     break;
                    case SP_QUAKEFIELD:  field_quake(&c->my_field);                  break;
                    case SP_GRAVITY:     field_gravity(&c->my_field);                break;
                    case SP_SWITCHFIELD:
                        /* Field swap is handled server-side; own new field arrives
                         * via "myfield" command; opponent's new field via "f" broadcast. */
                        break;
                    case SP_NUKEFIELD:   field_nuke(&c->my_field);                   break;
                    case SP_IMMUNITY:    c->immunity_timer=300; /* 5 sec at 60fps */ break;
                    case SP_CLEARCOLUMN: field_clear_column(&c->my_field);           break;
                    case SP_MUTATE:      c->mutate_timer=300;                        break;
                    case SP_DARKNESS:    c->darkness_timer=300;                      break;
                    case SP_CONFUSION:   c->confusion_timer=300;                     break;
                    default: break;
                }
                if(sp!=SP_IMMUNITY&&sp!=SP_MUTATE&&sp!=SP_DARKNESS&&sp!=SP_CONFUSION&&sp!=SP_SWITCHFIELD)
                    proto_send_field(c->sock,c->slot,&c->my_field);
            }
            sound_play(c,SFX_SPECIAL_RECV);
        }

        /* For opponent targets: field-modifying specials come via "f" broadcast.
         * Non-field specials (immunity, mutate, darkness, confusion) have no
         * visible effect on opponent display — nothing to do client-side. */

        static const struct { const char *verb; uint32_t atk_col; uint32_t frnd_col; }
        SP_DESC[NUM_SPECIALS+1]={
            {NULL,0,0},
            {"added garbage to",        0xff5050ff,0},          /* 1  a AddLine      */
            {"cleared a line for",      0,0x50ff80ff},          /* 2  c ClearLine    */
            {"turned specials to bricks on", 0xffcc40ff,0xffcc40ff},/* 3  b ClrSpecial*/
            {"randomised",              0xffaa40ff,0},          /* 4  r RandomClear  */
            {"bombed",                  0xffaa60ff,0},          /* 5  o BlockBomb    */
            {"quaked",                  0xff7070ff,0},          /* 6  q Quake        */
            {"applied gravity to",      0xff5050ff,0},          /* 7  g Gravity      */
            {"switched fields with",    0xff8040ff,0},          /* 8  s Switch       */
            {"nuked the field of",      0,0x50ff80ff},          /* 9  n Nuke         */
            {"granted immunity to",     0,0x80ffccff},          /* 10 i Immunity     */
            {"cleared a column on",     0xff6060ff,0},          /* 11 v ClearColumn  */
            {"mutated pieces for",      0xffaa40ff,0},          /* 12 m Mutate       */
            {"darkened the field of",   0xff8080ff,0},          /* 13 d Darkness     */
            {"confused controls of",    0xff7070ff,0},          /* 14 f Confusion    */
        };

        {
            const char *from_nick=(from_slot==c->slot)?c->nick:
                                  (from_slot>=1&&from_slot<=MAX_PLAYERS)?c->opp_nicks[from_slot-1]:"?";
            const char *to_nick  =(to_slot==c->slot)?c->nick:
                                  (to_slot>=1&&to_slot<=MAX_PLAYERS)?c->opp_nicks[to_slot-1]:"?";
            char act[MAX_ACT_LEN];
            uint32_t col=0xaaaaaaff;
            if(sp>=1&&sp<=NUM_SPECIALS){
                int self_target=(from_slot==to_slot);
                if(self_target)
                    snprintf(act,sizeof(act),"%s: %s (self)",from_nick,SP_DESC[sp].verb);
                else
                    snprintf(act,sizeof(act),"%s %s %s",from_nick,SP_DESC[sp].verb,to_nick);
                col=(to_slot==c->slot)?0xff5050ff:
                    (from_slot==c->slot)?0x50ff80ff:
                    0x888888ff;
                if(c->immunity_timer>0&&to_slot==c->slot&&sp!=SP_IMMUNITY){
                    strncat(act," [blocked]",sizeof(act)-strlen(act)-1);
                    col=0x80ff80ff;
                }
            } else {
                snprintf(act,sizeof(act),"%s -> %s [?]",from_nick,to_nick);
            }
            ctx_activity(c,act,col);
        }
        return;
    }
    if (!strcmp(cmd,"pline") && n>=3) {
        /* args[0]=nick, args[1..]=message words — reconstruct full message */
        char full_msg[MAX_CHAT_LEN]=""; int mi;
        for(mi=1;mi<n-1&&mi<15;mi++){
            if(mi>1) strncat(full_msg," ",sizeof(full_msg)-strlen(full_msg)-1);
            strncat(full_msg,args[mi],sizeof(full_msg)-strlen(full_msg)-1);
        }
        char buf[MAX_CHAT_LEN];
        snprintf(buf,sizeof(buf),"<%.31s> %.*s",args[0],(int)(sizeof(buf)-36),full_msg);
        ctx_chat(c,buf,COL_WHITE);
        sound_play(c,SFX_CHAT);
        return;
    }
    if (!strcmp(cmd,"taunt") && n>=3) {
        /* args[0]=sender nick, args[1]=target nick */
        static const char *taunts[] = {
            "Your stack looks like modern architecture!",
            "I've seen better builds from a blindfolded cat.",
            "Is that a tower or a cry for help?",
            "My grandma clears lines faster. She's been dead for years.",
            "I'd say keep trying but... no.",
            "That gap won't fill itself. Neither will your soul.",
            "I've seen smoother descents from a piano off a cliff.",
            "Bold strategy leaving that S-piece sideways. Bold.",
            "Your field is basically a modern art installation.",
            "Even the I-piece dispenser pities you.",
            "You play like you're doing it with your elbows.",
            "Is your keyboard okay? Asking for a friend.",
            "That tetromino placement was... a choice.",
            "Legend says that hole in your stack is still falling.",
            "Nice top-out incoming. I can feel it from here.",
        };
        static int taunt_idx = 0;
        static unsigned int taunt_seed = 0x4e657a7a;
        taunt_seed ^= taunt_seed << 13;
        taunt_seed ^= taunt_seed >> 17;
        taunt_seed ^= taunt_seed << 5;
        taunt_idx = (int)(taunt_seed % 15);
        snprintf(c->taunt_text, sizeof(c->taunt_text), "%.20s -> %.20s: %s",
                 args[0], args[1], taunts[taunt_idx]);
        c->taunt_timer = 280;
        sound_play(c, SFX_TAUNT);
        return;
    }
}

/* ============================================================
 * Key handler — Tab cycles: host → port → nick → pass → team → host
 * Nick enforces MAX_NICK_DISPLAY (8) char limit
 * ============================================================ */
void client_on_key(ClientCtx *c, int sc, int sym, int is_text, const char *txt) {
    /* Convert SDL_TEXTINPUT UTF-8 to Latin-1 so Swedish/accented chars
     * display correctly in input boxes (font uses Latin-1 indices). */
    char txt_l1[32]="";
    if (is_text && txt) { utf8_to_latin1(txt, txt_l1, sizeof(txt_l1)); txt = txt_l1; }
    #define SK_UP        82
    #define SK_DOWN      81
    #define SK_LEFT      80
    #define SK_RIGHT     79
    #define SK_RETURN    40
    #define SK_SPACE     44
    #define SK_ESCAPE    41
    #define SK_TAB       43
    #define SK_F5        62
    #define SK_BACKSPACE 42
    #define SK_1 30
    #define SK_2 31
    #define SK_3 32
    #define SK_4 33
    #define SK_5 34
    #define SK_6 35
    #define SK_7 36
    #define SK_8 37
    /* WASD scancodes (SDL2 scancodes for W/A/S/D) */
    #define SK_W  26
    #define SK_A   4
    #define SK_S  22
    #define SK_D   7
    #define SK_C   6   /* hold piece */
    #define SK_LSHIFT 225
    #define SK_RSHIFT 229
    /* Always support both WASD and arrow keys simultaneously */
    if(sc==SK_W) sc=SK_UP;
    else if(sc==SK_S) sc=SK_DOWN;
    else if(sc==SK_A) sc=SK_LEFT;
    else if(sc==SK_D) sc=SK_RIGHT;
    #define SK_9 38
    (void)sym;

    /* Connect screen text field input */
    if (c->state == CS_CONNECT) {
        TextField *focus = NULL;
        int is_nick = 0;
        if      (c->tf_host.focused) { focus=&c->tf_host; }
        else if (c->tf_port.focused) { focus=&c->tf_port; }
        else if (c->tf_nick.focused) { focus=&c->tf_nick; is_nick=1; }
        else if (c->tf_pass.focused) { focus=&c->tf_pass; }
        if (focus) {
            if (sc==SK_BACKSPACE) {
                if(focus->len>0) focus->text[--focus->len]='\0';
            } else if (sc==SK_RETURN) {
                client_on_button(c,BTN_CONNECT);
            } else if (sc==SK_TAB) {
                focus->focused=0;
                if      (focus==&c->tf_host) c->tf_port.focused=1;
                else if (focus==&c->tf_port) c->tf_nick.focused=1;
                else if (focus==&c->tf_nick) c->tf_pass.focused=1;
                else                         c->tf_host.focused=1;
            } else if (is_text&&txt) {
                int tl=(int)strlen(txt);
                int max_len = is_nick ? MAX_NICK_DISPLAY : (int)sizeof(focus->text)-1;
                if (focus->len+tl <= max_len) {
                    memcpy(focus->text+focus->len,txt,tl);
                    focus->len+=tl; focus->text[focus->len]='\0';
                }
            }
        } else if (sc==SK_RETURN) client_on_button(c,BTN_CONNECT);
        return;
    }

    /* Admin command input — takes priority over chat when active */
    /* Kick/ban popup — dismiss with Enter or Escape */
    if (c->kick_popup_open) {
        if (sc==SK_RETURN||sc==SK_ESCAPE) client_on_button(c,BTN_KICK_POPUP_OK);
        return;
    }

    /* Popup intercept — Escape or any key closes focused popup */
    if (c->game_options_popup_open) {
        if (sc==SK_ESCAPE) { c->game_options_popup_open=0; } return;
    }
    if (c->ai_players_popup_open) {
        if (sc==SK_ESCAPE) { c->ai_players_popup_open=0; } return;
    }
    if (c->license_popup_open) {
        if (sc==SK_UP)   { c->license_scroll--; if(c->license_scroll<0)c->license_scroll=0; }
        if (sc==SK_DOWN) { c->license_scroll++; }
        if (sc==SK_ESCAPE) { c->license_popup_open=0; }
        return;
    }
    if (c->keymap_popup_open) {
        if (c->keymap_capture_idx >= 0) {
            /* Any key press captures the new binding */
            if (sc != SK_ESCAPE) {
                int idx = c->keymap_capture_idx;
                int *km_fields[] = {&c->km.move_left,&c->km.move_right,
                    &c->km.rotate_cw,&c->km.rotate_ccw,&c->km.soft_drop,
                    &c->km.hard_drop,&c->km.hold,&c->km.hold2,
                    &c->km.chat,&c->km.pause,&c->km.ready};
                if (idx>=0 && idx<11) *km_fields[idx] = sc;
                settings_save(c);
            }
            c->keymap_capture_idx = -1;
            return;
        }
        if (sc==SK_ESCAPE) { c->keymap_popup_open=0; c->keymap_capture_idx=-1; }
        return;
    }

    /* Team name popup input */
    if (c->team_popup_open) {
        if (sc==SK_RETURN) {
            client_on_button(c, BTN_TEAM_POPUP_SET);
        } else if (sc==SK_ESCAPE) {
            c->team_popup_open=0;
        } else if (sc==SK_BACKSPACE) {
            if(c->tf_team_popup.len>0) c->tf_team_popup.text[--c->tf_team_popup.len]='\0';
        } else if (is_text&&txt) {
            int tl=(int)strlen(txt);
            /* Allow spaces; max 8 chars total */
            if (c->tf_team_popup.len+tl<=8) {
                memcpy(c->tf_team_popup.text+c->tf_team_popup.len,txt,tl);
                c->tf_team_popup.len+=tl; c->tf_team_popup.text[c->tf_team_popup.len]='\0';
            }
        }
        return;
    }

    /* MOTD popup input */
    if(c->motd_popup_open){
        TextField *tf=&c->tf_motd;
        if(sc==SK_RETURN){ client_on_button(c,BTN_MOTD_POPUP_OK); return; }
        if(sc==SK_ESCAPE){ c->motd_popup_open=0; return; }
        if(sc==SK_BACKSPACE){ if(tf->len>0) tf->text[--tf->len]='\0'; return; }
        if(is_text&&txt){ int tl=(int)strlen(txt);
            if(tf->len+tl<(int)sizeof(tf->text)-1){
                memcpy(tf->text+tf->len,txt,tl); tf->len+=tl; tf->text[tf->len]='\0';}
            return; }
        return;
    }

    /* Global chat input */
    if (c->schat_input.active) {
        if (sc==SK_RETURN) {
            if (c->schat_input.len>0) {
                /* All authenticated users can send server chat */
                char buf[MAX_MSG_LEN];
                snprintf(buf,sizeof(buf),"schat %s",c->schat_input.text);
                net_send(c->sock,buf);
                c->schat_input.len=0; c->schat_input.text[0]='\0';
            }
            c->schat_input.active=0;
        } else if (sc==SK_ESCAPE) {
            c->schat_input.active=0; c->schat_input.len=0; c->schat_input.text[0]='\0';
        } else if (sc==SK_BACKSPACE) {
            if(c->schat_input.len>0) c->schat_input.text[--c->schat_input.len]='\0';
        } else if (is_text&&txt) {
            int tl=(int)strlen(txt);
            if (c->schat_input.len+tl<CHAT_INPUT_MAX-1) {
                memcpy(c->schat_input.text+c->schat_input.len,txt,tl);
                c->schat_input.len+=tl; c->schat_input.text[c->schat_input.len]='\0';
            }
        }
        return;
    }

    /* chat input — handle before any state-specific checks */
    if (c->chat_input.active) {
        if (sc==SK_RETURN) {
            if (c->chat_input.len>0) {
                char buf[MAX_MSG_LEN];
                char *msg = c->chat_input.text;
                if (msg[0]=='/') {
                    if (strncasecmp(msg+1,"kick ",5)==0 &&
                        (c->priv_level<=2 || c->is_host || c->priv_level<=2)) {
                        /* /kick from room chat = room-kick, not server-kick.
                         * Send as gmsg so it goes through lobby_handle_message. */
                        snprintf(buf,sizeof(buf),"gmsg %s",msg);
                        net_send(c->sock,buf);
                    } else if (c->priv_level<=2 || c->priv_level<=2) {
                        /* Other admin commands go via schat */
                        snprintf(buf,sizeof(buf),"schat %s",msg);
                        net_send(c->sock,buf);
                    } else {
                        ctx_chat(c,"Only admins can use / commands",COL_INGAME_RED);
                    }
                } else {
                    /* Send pline — server echoes it back to all including sender */
                    snprintf(buf,sizeof(buf),"pline %d %s",c->slot,msg);
                    net_send(c->sock,buf);
                }
                c->chat_input.len=0; c->chat_input.text[0]='\0';
                c->chat_scroll=0; /* scroll to bottom on send */
            }
            c->chat_input.active=0;
        } else if (sc==SK_ESCAPE) {
            c->chat_input.active=0;
            c->chat_input.len=0; c->chat_input.text[0]='\0';
        } else if (sc==SK_BACKSPACE) {
            if(c->chat_input.len>0) c->chat_input.text[--c->chat_input.len]='\0';
        } else if (is_text&&txt) {
            int tl=(int)strlen(txt);
            if (c->chat_input.len+tl<CHAT_INPUT_MAX-1) {
                memcpy(c->chat_input.text+c->chat_input.len,txt,tl);
                c->chat_input.len+=tl;
                c->chat_input.text[c->chat_input.len]='\0';
            }
        }
        return;
    }

    if (c->state==CS_SERVER_LIST) {
        /* Popup text input routing */
        if (c->server_popup_open==1) {
            /* 4 fields: host, port, nick, pass (indices 0-3, no name field) */
            TextField *fields[4]={&c->tf_srv_host,&c->tf_srv_port,&c->tf_srv_nick,&c->tf_srv_pass};
            TextField *tf=fields[c->tf_srv_focus<4?c->tf_srv_focus:3];
            if(sc==SK_TAB){ c->tf_srv_focus=(c->tf_srv_focus+1)%4; return; }
            if(sc==SK_RETURN){ client_on_button(c,BTN_SRV_ADD_OK); return; }
            if(sc==SK_ESCAPE){ c->server_popup_open=0;
                if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
                c->srv_test_state=0; return; }
            if(sc==SK_BACKSPACE){ if(tf->len>0) tf->text[--tf->len]='\0'; return; }
            if(is_text&&txt){ int tl=(int)strlen(txt);
                if(tf->len+tl<(int)sizeof(tf->text)-1){
                    memcpy(tf->text+tf->len,txt,tl);
                    tf->len+=tl; tf->text[tf->len]='\0';} return; }
            return;
        }
        if (c->server_popup_open==3) {
            /* Rename popup — single field */
            TextField *tf=&c->tf_srv_rename;
            if(sc==SK_RETURN){ client_on_button(c,BTN_SRV_RENAME_OK); return; }
            if(sc==SK_ESCAPE){ c->server_popup_open=0; c->server_edit_idx=-1; return; }
            if(sc==SK_BACKSPACE){ if(tf->len>0) tf->text[--tf->len]='\0'; return; }
            if(is_text&&txt){ int tl=(int)strlen(txt);
                if(tf->len+tl<(int)sizeof(tf->text)-1){
                    memcpy(tf->text+tf->len,txt,tl);
                    tf->len+=tl; tf->text[tf->len]='\0';} return; }
            return;
        }
        if(sc==SK_RETURN||sc==SK_SPACE) client_on_button(c,BTN_SRV_CONNECT);
        else if(sc==SK_ESCAPE) {
            if(c->about_popup_open) c->about_popup_open=0;
            else if(c->server_popup_open) { c->server_popup_open=0; c->server_edit_idx=-1; }
        }
        else if(sc==SK_UP)  { if(c->server_cursor>0){ c->server_cursor--; c->srv_page=c->server_cursor/6; }}
        else if(sc==SK_DOWN){ if(c->server_cursor<c->server_count-1){ c->server_cursor++; c->srv_page=c->server_cursor/6; }}
        else if(sc==SK_LEFT) { if(c->srv_page>0){ c->srv_page--; c->server_cursor=c->srv_page*6; }}
        else if(sc==SK_RIGHT){ int mp=(c->server_count-1)/6; if(c->srv_page<mp){ c->srv_page++; c->server_cursor=c->srv_page*6; }}
        return;
    }
    if (c->state==CS_LOBBY_SELECT) {
        if (sc==SK_RETURN||sc==SK_SPACE) {
            if(c->lobby_list_count>0) ctx_join_lobby(c,c->lobby_list[c->lobby_cursor].id);
        } else if (sc==SK_UP) {
            if(c->lobby_cursor>0){
                c->lobby_cursor--; c->lobby_page=c->lobby_cursor/3;}
        } else if (sc==SK_DOWN) {
            if(c->lobby_cursor<c->lobby_list_count-1){
                c->lobby_cursor++; c->lobby_page=c->lobby_cursor/3;}
        } else if (sc==SK_LEFT) {
            if(c->lobby_page>0){
                c->lobby_page--; c->lobby_cursor=c->lobby_page*3;}
        } else if (sc==SK_RIGHT) {
            {int mp=(c->lobby_list_count-1)/3;
             if(c->lobby_page<mp){c->lobby_page++;c->lobby_cursor=c->lobby_page*3;}}
        } else if (sc>=SK_1&&sc<=SK_9) {
            int idx=c->lobby_page*3+(sc-SK_1);
            if(idx<c->lobby_list_count){
                c->lobby_cursor=idx;
                ctx_join_lobby(c,c->lobby_list[idx].id);}
        } else if (sc==SK_TAB) {
            client_on_button(c,BTN_GLOBAL_CHAT);
        } else if (sc==SK_ESCAPE) client_on_button(c,BTN_BACK_CONNECT);
        return;
    }

    if (c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER) {
        if (c->state==CS_IN_LOBBY) {
            /* R (or km.ready) — toggle ready only, never starts */
            if (sc==c->km.ready && !c->is_spectator)
                client_on_button(c,BTN_READY);
            /* Enter / Space / F5 — start game if host+ready, else toggle ready */
            else if (sc==SK_RETURN||sc==SK_F5||sc==SK_SPACE) {
                if ((c->is_host||c->priv_level<=2) && c->is_ready)
                    client_on_button(c,BTN_START_GAME);
                else if (!c->is_spectator)
                    client_on_button(c,BTN_READY);
            }
        }
        if (sc==SK_ESCAPE) client_on_button(c,BTN_BACK_TO_LOBBIES);
        else if (sc==SK_TAB) {
            client_on_button(c,BTN_OPEN_CHAT); /* always room chat inside a room */
        }
        return;
    }

    if (c->state==CS_PLAYING) {
        char buf[MAX_MSG_LEN];
        if (c->is_spectator) {
            if(sc==SK_TAB) client_on_button(c,BTN_OPEN_CHAT);
            else if(sc==SK_LEFT||sc==SK_RIGHT) {
                int dir=(sc==SK_RIGHT)?1:-1;
                int start=c->spec_watch_slot?c->spec_watch_slot:1, tries, s;
                for(tries=0;tries<MAX_PLAYERS;tries++){
                    start=((start-1+dir+MAX_PLAYERS)%MAX_PLAYERS)+1;
                    s=start-1;
                    if(s+1!=c->slot&&c->opp_nicks[s][0]){c->spec_watch_slot=s+1;break;}
                }
            }
            return;
        }
        if (c->paused) { if(sc==SK_F5) client_on_button(c,BTN_RESUME); return; }
        /* Confusion reverses left/right and swaps rotate direction */
        {int confused=(c->confusion_timer>0);
        /* Gameplay keys — use remappable keymap (km) instead of hardcoded scancodes */
        if (sc==c->km.move_left) {
                if(confused){ if(piece_move_right(&c->my_field,&c->cur)){sound_play(c,SFX_MOVE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
                else         { if(piece_move_left(&c->my_field,&c->cur)){sound_play(c,SFX_MOVE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
        } else if (sc==c->km.move_right) {
                if(confused){ if(piece_move_left(&c->my_field,&c->cur)){sound_play(c,SFX_MOVE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
                else         { if(piece_move_right(&c->my_field,&c->cur)){sound_play(c,SFX_MOVE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
        } else if (sc==c->km.rotate_cw) {
                if(confused){ if(piece_rotate_ccw(&c->my_field,&c->cur)){sound_play(c,SFX_ROTATE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
                else         { if(piece_rotate_cw(&c->my_field,&c->cur)){sound_play(c,SFX_ROTATE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
        } else if (sc==c->km.rotate_ccw) {
                if(confused){ if(piece_rotate_cw(&c->my_field,&c->cur)){sound_play(c,SFX_ROTATE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
                else         { if(piece_rotate_ccw(&c->my_field,&c->cur)){sound_play(c,SFX_ROTATE);proto_send_field(c->sock,c->slot,&c->my_field);c->lock_timer=c->lock_delay_ms;} }
        } else if (sc==c->km.soft_drop) {
                if(piece_move_down(&c->my_field,&c->cur)){c->drop_timer=c->drop_interval;c->lock_active=0;proto_send_field(c->sock,c->slot,&c->my_field);}
        } else if (sc==c->km.hard_drop) {
                piece_drop(&c->my_field,&c->cur);
                c->lock_active=0;
                lock_and_spawn(c);
                net_send(c->sock,"holdclear");
        } else if (sc==c->km.hold||sc==c->km.hold2||sc==SK_RSHIFT) {
            /* Hold piece — swap cur with hold slot */
            if (c->hold_enabled && !c->hold_used) {
                c->hold_used = 1;
                c->lock_active = 0; c->lock_timer = 0;
                int prev_hold_type = c->hold.type;
                c->hold.type = c->cur.type;
                { char hbuf[32]; snprintf(hbuf,sizeof(hbuf),"holdpiece %d",c->hold.type);
                  net_send(c->sock,hbuf); }
                if (prev_hold_type >= 0) {
                    piece_init(&c->cur, prev_hold_type);
                    piece_spawn(&c->my_field, &c->cur);
                } else {
                    c->cur = c->next;
                    if (c->srv_piece_head < c->srv_piece_tail)
                        piece_init(&c->next, c->srv_pieces[c->srv_piece_head++ % 128]);
                    else { int freq[NUM_PIECES]={14,14,14,14,14,14,14}; piece_random(&c->next,freq); }
                    if (!piece_spawn(&c->my_field,&c->cur)) { lock_and_spawn(c); }
                }
                c->drop_timer = c->drop_interval;
                proto_send_field(c->sock,c->slot,&c->my_field);
            }
        } else if (sc==c->km.chat) {
            client_on_button(c,BTN_OPEN_CHAT);
        } else if (sc==c->km.pause) { client_on_button(c,BTN_PAUSE);
        } else if (sc==SK_ESCAPE) { client_on_button(c,BTN_BACK_TO_LOBBIES);
        } else if (sc>=SK_1&&sc<=SK_8) {
        {
            int disp_key = sc - SK_1 + 1; /* 1=self, 2=first opp, etc. */
                /* Map display key → actual server slot */
                int t;
                if (disp_key == 1) {
                    t = c->slot; /* send to self */
                } else {
                    /* Find the (disp_key-2)th opponent in slot order */
                    int n2=0, oi;
                    t = -1;
                    for(oi=0;oi<MAX_PLAYERS;oi++){
                        if(oi+1==c->slot) continue;
                        if(c->opp_nicks[oi][0]){
                            n2++;
                            if(n2==disp_key-1){ t=oi+1; break; }
                        }
                    }
                }
                if (t > 0) {
                    c->attack_target=t;
                    if (!c->no_attacks&&c->my_field.special_count>0) {
                        uint8 sp=field_pop_special(&c->my_field);
                        if (sp) {
                            proto_send_field(c->sock,c->slot,&c->my_field);
                            snprintf(buf,sizeof(buf),"sb %d %c %d",t,SPECIAL_CHARS[sp-1],c->slot);
                            net_send(c->sock,buf);
                            sound_play(c,SFX_SPECIAL_SEND);
                        }
                    }
                }
            } /* end number keys block */
            } /* end sc>=SK_1 */
        } /* end if/else km chain */
        } /* end confused block */
    }

/* ============================================================
 * Button dispatcher
 * ============================================================ */
void client_on_button(ClientCtx *c, BtnId btn) {
    char buf[MAX_MSG_LEN];
    switch(btn) {
    case BTN_CONNECT: {
        const char *host = c->tf_host.text[0] ? c->tf_host.text : "localhost";
        int port = c->tf_port.text[0] ? atoi(c->tf_port.text) : DEFAULT_PORT;
        const char *nick = c->tf_nick.text[0] ? c->tf_nick.text : NULL;
        const char *pass = c->tf_pass.text;
        if (port<=0||port>65535) port=DEFAULT_PORT;
        if (!nick) {
            snprintf(c->connect_error,sizeof(c->connect_error),"Username required");
            break;
        }
        if ((int)strlen(nick) > MAX_NICK_DISPLAY) {
            snprintf(c->connect_error,sizeof(c->connect_error),"Username max %d chars",MAX_NICK_DISPLAY);
            break;
        }
        strncpy(c->nick, nick, MAX_NICK_LEN-1);
        c->team[0] = '\0';

        if (pass[0]) {
            client_sha256_hex(pass, c->auth_pass_hash);
            c->have_saved_creds = 1;
            strncpy(c->saved_nick, nick, MAX_NICK_LEN-1);
            strncpy(c->saved_pw_hash, c->auth_pass_hash, 64);
            /* Store hash in matching server list entry — only persistence needed */
            {int si; for(si=0;si<c->server_count;si++){
                if(!strcasecmp(c->server_list[si].host,host)&&c->server_list[si].port==port){
                    strncpy(c->server_list[si].nick,nick,MAX_NICK_LEN-1);
                    strncpy(c->server_list[si].pw_hash,c->auth_pass_hash,64);
                    server_list_save(c); break;}}}
        } else if (c->have_saved_creds &&
                   strcmp(c->saved_nick, nick) == 0 &&
                   c->auth_pass_hash[0] != '\0') {
            /* Blank password + same nick → use stored hash silently */
        } else {
            /* Try hash from server list entry */
            int si; int found=0;
            for(si=0;si<c->server_count;si++){
                if(!strcasecmp(c->server_list[si].host,host)&&
                   c->server_list[si].port==port&&
                   !strcasecmp(c->server_list[si].nick,nick)&&
                   c->server_list[si].pw_hash[0]){
                    strncpy(c->auth_pass_hash,c->server_list[si].pw_hash,64);
                    c->have_saved_creds=1;
                    strncpy(c->saved_nick,nick,MAX_NICK_LEN-1);
                    strncpy(c->saved_pw_hash,c->auth_pass_hash,64);
                    found=1; break;}
            }
            if(!found){
                snprintf(c->connect_error,sizeof(c->connect_error),"Password required");
                break;
            }
        }

        strncpy(c->auth_nick, nick, MAX_NICK_LEN-1);
        c->connect_error[0] = '\0';
        if (c->sock != INVALID_SOCK) { sock_close(c->sock); c->sock=INVALID_SOCK; }
        c->sock = net_connect(host, port);
        if (c->sock==INVALID_SOCK) {
            char htrunc[80]; strncpy(htrunc,host,79); htrunc[79]='\0';
            snprintf(c->connect_error,sizeof(c->connect_error),"Cannot connect to %s:%d",htrunc,port);
        } else {
            net_set_nonblocking(c->sock);
            c->state = CS_AUTH;
            c->lobby_list_count = 0;
            c->lobby_cursor = 0;
            c->got_lobby_list = 0;
            c->pending_lobby_id = 1;
            c->auth_nick_is_new = 0;
            c->auth_nonce[0] = '\0';
        }
        break;
    }
    case BTN_QUIT:         c->running=0; break;

    case BTN_SRV_ABOUT:
        c->about_popup_open=1;
        break;
    case BTN_SRV_REFRESH:
        if (c->server_cursor>=0&&c->server_cursor<c->server_count) {
            /* Close any existing probe */
            if(c->srv_test_sock!=INVALID_SOCK&&c->srv_test_state==10){
                sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK;
                c->srv_test_state=0;
            }
            if(c->srv_test_state==0){
                SavedServer *ps=&c->server_list[c->server_cursor];
                ps->rooms_playing=0; ps->rooms_total=0;
                ps->users_playing=0; ps->users_idle=0;
                c->srv_test_recv_len=0; c->srv_test_ticks=0;
                c->srvlist_probe_idx=c->server_cursor;
                c->srv_test_sock=net_connect_nb(ps->host,ps->port);
                if(c->srv_test_sock==INVALID_SOCK){
                    ps->online=-1; ps->auth_status=-3;
                } else {
                    c->srv_test_state=10;
                    ps->online=1;
                }
                c->srvlist_probe_timer=300;
            }
        }
        break;
    case BTN_SRV_ADD:
        c->server_popup_open=1;
        c->server_edit_idx=-1;
        memset(&c->tf_srv_host,0,sizeof(c->tf_srv_host));
        memset(&c->tf_srv_port,0,sizeof(c->tf_srv_port));
        memset(&c->tf_srv_nick,0,sizeof(c->tf_srv_nick));
        memset(&c->tf_srv_pass,0,sizeof(c->tf_srv_pass));
        strncpy(c->tf_srv_port.text,"31457",sizeof(c->tf_srv_port.text)-1);
        c->tf_srv_port.len=5;
        c->tf_srv_focus=0;
        c->srv_test_state=0; c->srv_test_msg[0]='\0'; c->srv_test_sname[0]='\0';
        if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
        break;

    case BTN_SRV_EDIT:
        /* Rename only — single alias field popup */
        if (c->server_cursor>=0&&c->server_cursor<c->server_count) {
            c->server_popup_open=3;
            c->server_edit_idx=c->server_cursor;
            memset(&c->tf_srv_rename,0,sizeof(c->tf_srv_rename));
            strncpy(c->tf_srv_rename.text,c->server_list[c->server_cursor].name,
                    sizeof(c->tf_srv_rename.text)-1);
            c->tf_srv_rename.len=(int)strlen(c->tf_srv_rename.text);
            c->tf_srv_rename.focused=1;
        }
        break;

    case BTN_SRV_RENAME_OK:
        if(c->server_edit_idx>=0&&c->server_edit_idx<c->server_count){
            if(c->tf_srv_rename.len>0)
                strncpy(c->server_list[c->server_edit_idx].name,
                        c->tf_srv_rename.text,MAX_SERVER_NAME-1);
            server_list_save(c);
        }
        c->server_popup_open=0; c->server_edit_idx=-1;
        break;

    case BTN_SRV_ADD_OK:
        /* Start async test-connect — does NOT block the render loop */
        if(c->tf_srv_host.len>0&&c->tf_srv_nick.len>0&&c->tf_srv_pass.len>0){
            if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
            c->srv_test_state=1;   /* state 1 = TCP connecting in progress */
            c->srv_test_ticks=0;
            c->srv_test_recv_len=0;
            c->srv_test_timeout = 60*3; /* ~3 seconds at 60fps */
            client_sha256_hex(c->tf_srv_pass.text,c->srv_test_hash);
            strncpy(c->srv_test_msg,"Connecting...",sizeof(c->srv_test_msg)-1);
            {int port=c->tf_srv_port.len>0?atoi(c->tf_srv_port.text):DEFAULT_PORT;
             /* net_connect_nb: DNS (fast for IPs) + non-blocking connect() */
             c->srv_test_sock=net_connect_nb(c->tf_srv_host.text,port);
             if(c->srv_test_sock==INVALID_SOCK){
                 c->srv_test_state=-1;
                 strncpy(c->srv_test_msg,"Host not found",sizeof(c->srv_test_msg)-1);
             }}
        } else {
            c->srv_test_state=-1;
            strncpy(c->srv_test_msg,
                c->tf_srv_host.len==0?"Host / IP required":
                c->tf_srv_nick.len==0?"Username required":"Password required",
                sizeof(c->srv_test_msg)-1);
        }
        break;

    case BTN_SRV_DEL:
        if (c->server_cursor>=0&&c->server_cursor<c->server_count) {
            int i;
            for(i=c->server_cursor;i<c->server_count-1;i++)
                c->server_list[i]=c->server_list[i+1];
            c->server_count--;
            if(c->server_cursor>=c->server_count) c->server_cursor=c->server_count-1;
            server_list_save(c);
        }
        break;

    case BTN_SRV_CONNECT:
        if (c->server_cursor>=0&&c->server_cursor<c->server_count) {
            SavedServer *ss=&c->server_list[c->server_cursor];
            /* Block only if explicitly failed auth/unreachable */
            if(ss->auth_status==-1||ss->auth_status==-2||ss->auth_status==-3) break;
            strncpy(c->tf_host.text,ss->host,sizeof(c->tf_host.text)-1);
            c->tf_host.len=(int)strlen(ss->host);
            { char ps[8]; snprintf(ps,sizeof(ps),"%d",ss->port);
              strncpy(c->tf_port.text,ps,sizeof(c->tf_port.text)-1);
              c->tf_port.len=(int)strlen(ps); }
            if(ss->nick[0]){
                strncpy(c->tf_nick.text,ss->nick,sizeof(c->tf_nick.text)-1);
                c->tf_nick.len=(int)strlen(ss->nick);}
            if(ss->pw_hash[0]){
                strncpy(c->auth_pass_hash,ss->pw_hash,64);
                strncpy(c->saved_nick,ss->nick,MAX_NICK_LEN-1);
                strncpy(c->saved_pw_hash,ss->pw_hash,64);
                c->have_saved_creds=1;
                c->tf_pass.len=0; c->tf_pass.text[0]='\0';
            }
            client_on_button(c,BTN_CONNECT);
        }
        break;
        case BTN_TOGGLE_SOUND:   c->sound_enabled=!c->sound_enabled; settings_save(c); break;
        case BTN_TOGGLE_MUSIC:   music_set_enabled(!music_get_enabled()); settings_save(c); break;
    case BTN_BACK_CONNECT:
        if (c->sock!=INVALID_SOCK) { sock_close(c->sock); c->sock=INVALID_SOCK; }
        if (c->srv_test_sock!=INVALID_SOCK) { sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK; }
        c->srv_test_state=0; c->srvlist_probe_timer=1; c->srvlist_probe_idx=0;
        ctx_reset_lobby(c);
        c->is_admin=0; c->priv_level=0;
        c->state = CS_SERVER_LIST;
        c->lobby_list_count=0;
        music_load("lineclash_menu.mod");
        break;
    case BTN_LOBBY_JOIN:
        if (c->lobby_list_count>0) ctx_join_lobby(c,c->lobby_list[c->lobby_cursor].id);
        break;
    case BTN_LOBBY_ROW_0: case BTN_LOBBY_ROW_1: case BTN_LOBBY_ROW_2: case BTN_LOBBY_ROW_3:
    case BTN_LOBBY_ROW_4: case BTN_LOBBY_ROW_5: case BTN_LOBBY_ROW_6: case BTN_LOBBY_ROW_7:
    case BTN_LOBBY_ROW_8: case BTN_LOBBY_ROW_9: {
        int idx=(int)(btn-BTN_LOBBY_ROW_0);
        if (idx<c->lobby_list_count) {
            if (c->lobby_cursor==idx) ctx_join_lobby(c,c->lobby_list[idx].id);
            else c->lobby_cursor=idx;
        }
        break;
    }
    case BTN_START_GAME:
        if (c->is_host||c->priv_level<=2) net_send(c->sock,"startgame 1");
        else ctx_status(c,"Only the host can start");
        break;
    case BTN_READY:
        if (!c->is_spectator) net_send(c->sock,"playerready");
        break;
    case BTN_BACK_TO_LOBBIES:
        net_send(c->sock,"leaveLobby");
        ctx_reset_lobby(c); c->state=CS_LOBBY_SELECT;
        c->lobby_list_count=0; c->got_lobby_list=0;
        c->lobby_refresh_timer=10; /* refresh quickly */
        net_send(c->sock,"listlobbies"); /* explicit request in case server auto-send missed */
        net_send(c->sock,"reqleaderboard");
        music_load("lineclash_menu.mod");
        break;
    case BTN_ADD_BOT_EASY:
    case BTN_ADD_BOT_MEDIUM:
    case BTN_ADD_BOT_HARD:
        if(c->bots_allowed && (c->is_host || c->priv_level<=2)) {
            /* Count current occupants: self + all opponents */
            int np=1;
            for(int bi=0;bi<MAX_PLAYERS;bi++) if(c->opp_nicks[bi][0]) np++;
            if(np < MAX_PLAYERS) {
                const char *diff = (btn==BTN_ADD_BOT_EASY)   ? "easy" :
                                   (btn==BTN_ADD_BOT_MEDIUM) ? "medium" : "hard";
                snprintf(buf,sizeof(buf),"addbot %s",diff);
                net_send(c->sock,buf);
            }
        }
        break;
    case BTN_REMOVE_BOT:
        if(c->bots_allowed && (c->is_host || c->priv_level<=2)) {
            /* Remove first bot found */
            for(int bi=0;bi<MAX_PLAYERS;bi++){
                if(c->opp_is_bot[bi]){
                    char rmsg[64]; snprintf(rmsg,sizeof(rmsg),"removebot %d",bi+1);
                    net_send(c->sock,rmsg);
                    break; /* one at a time */
                }
            }
        }
        break;
    case BTN_OPEN_CHAT:
        c->chat_input.active=1; c->chat_input.len=0; c->chat_input.text[0]='\0';
        sdl_text_start(); /* ensure text input active on macOS */
        break;
    case BTN_TOGGLE_ATTACKS:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->no_attacks=!c->no_attacks;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_TOGGLE_ENDLESS:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->last_player_wins=!c->last_player_wins;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_TOGGLE_TAUNTS:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->no_taunts=!c->no_taunts;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_TOGGLE_SPEEDUP:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->speedup_enabled=!c->speedup_enabled;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_TEAM_POPUP:
        /* Copy current team into popup field and open */
        strncpy(c->tf_team_popup.text, c->team, sizeof(c->tf_team_popup.text)-1);
        c->tf_team_popup.len = (int)strlen(c->tf_team_popup.text);
        c->team_popup_open = 1;
        break;
    case BTN_KICK_POPUP_OK:
        c->kick_popup_open=0;
        c->kick_popup_msg[0]='\0';
        if (c->kick_popup_room_return) {
            c->kick_popup_room_return=0;
            ctx_reset_lobby(c);
            c->state=CS_LOBBY_SELECT;
            c->lobby_list_count=0;
            c->got_lobby_list=0;
            c->lobby_refresh_timer=10; /* refresh shortly */
        }
        break;
    case BTN_TEAM_POPUP_SET:
        c->team_popup_open = 0;
        /* Copy popup text into team field and send */
        strncpy(c->tf_team_lobby.text, c->tf_team_popup.text, sizeof(c->tf_team_lobby.text)-1);
        c->tf_team_lobby.len = (int)strlen(c->tf_team_lobby.text);
        client_on_button(c, BTN_SET_TEAM);
        break;
    case BTN_MOTD_EDIT:
        c->motd_popup_open=1;
        memset(&c->tf_motd,0,sizeof(c->tf_motd));
        strncpy(c->tf_motd.text,c->motd,sizeof(c->tf_motd.text)-1);
        c->tf_motd.len=(int)strlen(c->tf_motd.text);
        c->tf_motd.focused=1;
        break;
    case BTN_KEYMAP_POPUP:
        c->keymap_popup_open = !c->keymap_popup_open;
        c->keymap_capture_idx = -1;  /* never start in capture mode */
        c->game_options_popup_open = 0;
        c->ai_players_popup_open = 0;
        break;
    case BTN_GAME_OPTIONS:
        c->game_options_popup_open = !c->game_options_popup_open;
        c->ai_players_popup_open = 0;
        c->keymap_popup_open = 0;
        break;
    case BTN_TOGGLE_HOLD:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->hold_enabled=!c->hold_enabled;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_AI_PLAYERS:
        c->ai_players_popup_open = !c->ai_players_popup_open;
        c->game_options_popup_open = 0;
        break;
    case BTN_TOGGLE_GARBAGE_SYSTEM:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->garbage_system = c->garbage_system ? 0 : 1;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_TOGGLE_GARBAGE_TARGET:
        if ((c->is_host||c->priv_level<=2)&&(c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER)) {
            c->garbage_targeting = (c->garbage_targeting + 1) % 3;
            snprintf(buf,sizeof(buf),"setsettings %d %d %d %d %d %d %d",
                     c->no_attacks,c->last_player_wins,c->no_taunts,c->speedup_enabled,
                     c->garbage_system,c->garbage_targeting,c->hold_enabled);
            net_send(c->sock,buf);
        }
        break;
    case BTN_MOTD_POPUP_OK:
        if(c->motd_popup_open&&c->tf_motd.len>0){
            /* Send chmotd command — server updates live and saves to config */
            char mbuf[MAX_MSG_LEN];
            snprintf(mbuf,sizeof(mbuf),"schat /chmotd \"%s\"",c->tf_motd.text);
            net_send(c->sock,mbuf);
            strncpy(c->motd,c->tf_motd.text,sizeof(c->motd)-1);
        }
        c->motd_popup_open=0;
        break;
    case BTN_GLOBAL_CHAT:
        /* All authenticated users can use the server chat */
        c->schat_input.active = 1;
        if(c->schat_input.len==0) c->schat_input.text[0]='\0';
        sdl_text_start();
        break;
    case BTN_TAUNT:
        if (c->state==CS_PLAYING||c->state==CS_IN_LOBBY||c->state==CS_GAMEOVER) {
            if (c->taunt_cooldown>0) break;
            {
                const char *targets[MAX_PLAYERS]; int tc=0, ri;
                for (ri=0;ri<MAX_PLAYERS;ri++)
                    if (ri+1!=c->slot&&c->opp_nicks[ri][0]) targets[tc++]=c->opp_nicks[ri];
                if (tc>0) {
                    const char *tgt = targets[rand()%tc];
                    snprintf(buf,sizeof(buf),"taunt %s",tgt);
                    net_send(c->sock,buf);
                    c->taunt_cooldown = 300;
                }
            }
        }
        break;
    case BTN_TARGET_1: case BTN_TARGET_2: case BTN_TARGET_3: case BTN_TARGET_4:
    case BTN_TARGET_5: case BTN_TARGET_6: case BTN_TARGET_7: case BTN_TARGET_8: {
        int t=(int)(btn-BTN_TARGET_1)+1;
        if (t==c->slot) break;
        c->attack_target=t;
        if (!c->no_attacks&&c->state==CS_PLAYING&&!c->paused&&c->my_field.special_count>0) {
            uint8 sp=field_pop_special(&c->my_field);
            if (sp) {
                proto_send_field(c->sock,c->slot,&c->my_field);
                snprintf(buf,sizeof(buf),"sb %d %c %d",t,SPECIAL_CHARS[sp-1],c->slot);
                net_send(c->sock,buf);
                sound_play(c,SFX_SPECIAL_SEND);
            }
        }
        break;
    }
    case BTN_FIRE_SPECIAL:
        /* Fire at the currently selected attack target (c->attack_target).
         * Key 1 = own field (c->slot), keys 2-8 = opponents in display order.
         * All specials including friendly ones can target any slot. */
        if (!c->no_attacks&&c->state==CS_PLAYING&&!c->paused
            &&c->attack_target>0&&c->my_field.special_count>0) {
            uint8 sp=field_pop_special(&c->my_field);
            if (sp) {
                proto_send_field(c->sock,c->slot,&c->my_field);
                snprintf(buf,sizeof(buf),"sb %d %c %d",c->attack_target,SPECIAL_CHARS[sp-1],c->slot);
                net_send(c->sock,buf);
                sound_play(c,SFX_SPECIAL_SEND);
            }
        }
        break;
    case BTN_SET_TEAM: {
        const char *t = c->tf_team_lobby.text;
        strncpy(c->team, t, MAX_NICK_LEN-1);
        snprintf(buf, sizeof(buf), "setteam %s", t[0] ? t : "-");
        net_send(c->sock, buf);
        break;
    }
    case BTN_PAUSE:
        if (c->state==CS_PLAYING&&!c->paused) {
            snprintf(buf,sizeof(buf),"pause 1 %d",c->slot);
            net_send(c->sock,buf);
        }
        break;
    case BTN_RESUME:
        if (c->state==CS_PLAYING&&c->paused) {
            snprintf(buf,sizeof(buf),"pause 0 %d",c->slot);
            net_send(c->sock,buf);
        }
        break;
    case BTN_END_GAME:
        if (c->state==CS_PLAYING)
            net_send(c->sock,"endgame");
        break;
    default: break;
    }
}

/* ============================================================
 * Tick
 * ============================================================ */
void client_tick(ClientCtx *c) {
    int i;
    for (i=0;i<c->chat_count;i++) c->chat[i].age++;
    for (i=0;i<c->schat_count;i++) c->schat[i].age++;
    for (i=0;i<c->act_count;i++)  c->act_log[i].age++;
    if(c->immunity_timer>0)  c->immunity_timer--;
    if(c->darkness_timer>0)  c->darkness_timer--;
    if(c->confusion_timer>0) c->confusion_timer--;
    if(c->mutate_timer>0)    c->mutate_timer--;

    /* Test-connect probe for add-server popup */
    /* State 1: TCP connect in progress — poll each frame, no blocking */
    if(c->srv_test_state==1){
        c->srv_test_timeout--;
        int pr=net_connect_nb_poll(c->srv_test_sock);
        if(pr==1){
            /* TCP connected — advance to reading server greeting */
            c->srv_test_state=2;
            c->srv_test_ticks=0;
            strncpy(c->srv_test_msg,"Connected, waiting...",sizeof(c->srv_test_msg)-1);
        } else if(pr<0 || c->srv_test_timeout<=0){
            c->srv_test_state=-1;
            strncpy(c->srv_test_msg,
                c->srv_test_timeout<=0?"Connection timed out":"Cannot connect to server",
                sizeof(c->srv_test_msg)-1);
            if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
        }
        /* pr==0: still connecting, keep waiting */
    }
    /* State 2/3: connected — read server messages and authenticate */
    if(c->srv_test_state==2||c->srv_test_state==3){
        c->srv_test_ticks++;
        if(c->srv_test_ticks>300){ /* 5 sec timeout */
            c->srv_test_state=-1;
            strncpy(c->srv_test_msg,"Connection timed out",sizeof(c->srv_test_msg)-1);
            if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
        } else {
            char tline[MAX_MSG_LEN];
            int r=net_recv(c->srv_test_sock,c->srv_test_recv,&c->srv_test_recv_len,
                           tline,sizeof(tline));
            if(r<0){
                c->srv_test_state=-1;
                strncpy(c->srv_test_msg,"Connection lost",sizeof(c->srv_test_msg)-1);
                if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
            } else if(r==1&&tline[0]){
                char tcmd[64],targs[4][MAX_MSG_LEN];
                proto_parse(tline,tcmd,targs,4);
                if(!strcmp(tcmd,"noconnecting")){
                    strncpy(c->srv_test_msg,"Authenticating...",sizeof(c->srv_test_msg)-1);
                    c->srv_test_state=3;
                } else if(!strcmp(tcmd,"servername")){
                    strncpy(c->srv_test_sname,targs[0],MAX_SERVER_NAME-1);
                } else if(!strcmp(tcmd,"lobbylistend")){
                    /* send authnick */
                    char tbuf[MAX_MSG_LEN];
                    snprintf(tbuf,sizeof(tbuf),"authnick %s",c->tf_srv_nick.text);
                    net_send(c->srv_test_sock,tbuf);
                } else if(!strcmp(tcmd,"authchallenge")){
                    char tresp[65],tbuf[MAX_MSG_LEN];
                    if(!strcmp(targs[0],"new")){
                        strncpy(tresp,c->srv_test_hash,65);
                    } else {
                        client_hmac_sha256_hex(c->srv_test_hash,targs[0],tresp);
                    }
                    snprintf(tbuf,sizeof(tbuf),"authresp %s",tresp);
                    net_send(c->srv_test_sock,tbuf);
                } else if(!strcmp(tcmd,"authok")){
                    /* Success — save the server entry */
                    int port=c->tf_srv_port.len>0?atoi(c->tf_srv_port.text):DEFAULT_PORT;
                    if(c->server_count<MAX_SAVED_SERVERS){
                        SavedServer *ns=&c->server_list[c->server_count++];
                        memset(ns,0,sizeof(*ns));
                        strncpy(ns->host,c->tf_srv_host.text,127);
                        ns->port=port;
                        strncpy(ns->nick,c->tf_srv_nick.text,MAX_NICK_LEN-1);
                        /* Use server name if we got one, else host */
                        if(c->srv_test_sname[0])
                            strncpy(ns->name,c->srv_test_sname,MAX_SERVER_NAME-1);
                        else
                            strncpy(ns->name,ns->host,MAX_SERVER_NAME-1);
                        strncpy(ns->pw_hash,c->srv_test_hash,64);
                        ns->auth_status=1; ns->online=1;
                        c->server_cursor=c->server_count-1;
                        server_list_save(c);
                    }
                    sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK;
                    c->srv_test_state=0;  /* reset so probe can start */
                    c->server_popup_open=0;
                } else if(!strcmp(tcmd,"autherr")){
                    snprintf(c->srv_test_msg,sizeof(c->srv_test_msg),"Auth failed: %.80s",
                             targs[0][0]?targs[0]:"wrong password or user not found");
                    c->srv_test_state=-1;
                    if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
                }
            }
        }
    }
    /* Auto-refresh server list: async non-blocking probe, one server at a time.
       Timer starts at 1 so first probe fires on entry. */
    if(c->state==CS_SERVER_LIST&&c->server_count>0&&c->server_popup_open==0){
        /* --- If a probe socket is active, read from it --- */
        if(c->srv_test_sock!=INVALID_SOCK&&c->srv_test_state==10){
            int pi=c->srvlist_probe_idx;
            SavedServer *ps=(pi>=0&&pi<c->server_count)?&c->server_list[pi]:NULL;
            c->srv_test_ticks++;
            if(c->srv_test_ticks>180||!ps){ /* 3s timeout */
                if(ps&&ps->online!=1){ ps->online=-1; ps->auth_status=-3;
                    ps->rooms_playing=0; ps->rooms_total=0;
                    ps->users_playing=0; ps->users_idle=0; }
                sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK;
                c->srv_test_state=0;
                c->srvlist_probe_idx=(pi+1)%c->server_count;
                c->srvlist_probe_timer=1;
            } else {
                /* Drain ALL available lines this tick */
                int done=0;
                while(!done){
                    char tline[MAX_MSG_LEN];
                    int r=net_recv(c->srv_test_sock,c->srv_test_recv,
                                   &c->srv_test_recv_len,tline,sizeof(tline));
                    if(r<0){
                        if(ps){ ps->online=-1; ps->auth_status=-3; }
                        sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK;
                        c->srv_test_state=0;
                        c->srvlist_probe_idx=(pi+1)%c->server_count;
                        c->srvlist_probe_timer=1;
                        done=1;
                    } else if(r==1&&tline[0]){
                        char tcmd[64],targs[17][MAX_MSG_LEN];
                        proto_parse(tline,tcmd,targs,16);
                        if(!strcmp(tcmd,"noconnecting")&&ps)      { ps->online=1; ps->auth_status=0; }
                        else if(!strcmp(tcmd,"servername")&&ps){
                            if(!strcmp(ps->name,ps->host))
                                strncpy(ps->name,targs[0],MAX_SERVER_NAME-1);
                        } else if(!strcmp(tcmd,"usercount")&&ps)  ps->users_registered=atoi(targs[0]);
                        else if(!strcmp(tcmd,"conncount")&&ps){
                            /* Store conncount — defer idle calc until lobbylistend */
                            c->srv_test_conncount=atoi(targs[0]);
                        } else if(!strcmp(tcmd,"lobbyinfo")&&ps){
                            int in_game=(strcmp(targs[3],"ingame")==0);
                            if(in_game) ps->users_playing+=atoi(targs[1]);
                            ps->rooms_total++;
                            if(in_game) ps->rooms_playing++;
                        } else if(!strcmp(tcmd,"lobbylistend")){
                            if(ps){
                                ps->online=1;
                                /* Now calculate idle: conncount - playing */
                                int conn=c->srv_test_conncount;
                                ps->users_idle=conn>ps->users_playing?conn-ps->users_playing:0;
                            }
                            sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK;
                            c->srv_test_state=0;
                            c->srvlist_probe_idx=(pi+1)%c->server_count;
                            /* 5s pause after completing a full cycle */
                            c->srvlist_probe_timer=(c->srvlist_probe_idx==0)?300:1;
                            done=1;
                        }
                        /* else: ignore unknown lines, keep reading */
                    } else {
                        done=1; /* no more data right now */
                    }
                }
            }
        }
        /* --- Start next probe when timer fires and no probe active --- */
        c->srvlist_probe_timer--;
        if(c->srvlist_probe_timer<=0
           && c->srv_test_sock==INVALID_SOCK
           && c->srv_test_state==0){
            int pi=c->srvlist_probe_idx;
            if(pi<0||pi>=c->server_count) pi=c->srvlist_probe_idx=0;
            SavedServer *ps=&c->server_list[pi];
            ps->rooms_playing=0; ps->rooms_total=0;
            ps->users_playing=0; ps->users_idle=0;
            c->srv_test_conncount=0;
            c->srv_test_recv_len=0; c->srv_test_ticks=0;
            c->srv_test_sock=net_connect_nb(ps->host,ps->port);
            if(c->srv_test_sock==INVALID_SOCK){
                ps->online=-1; ps->auth_status=-3;
                c->srvlist_probe_idx=(pi+1)%c->server_count;
                c->srvlist_probe_timer=(c->srvlist_probe_idx==0)?300:1;
            } else {
                c->srv_test_state=10;
                /* Don't set online=1 yet — wait for noconnecting */
            }
        }
    } else if(c->state!=CS_SERVER_LIST){
        /* Always clean probe state when not in server list */
        if(c->srv_test_sock!=INVALID_SOCK&&c->srv_test_state==10){
            sock_close(c->srv_test_sock); c->srv_test_sock=INVALID_SOCK;
        }
        /* Reset state to 0 unless the add-server popup test is actively running */
        if(c->srv_test_state!=1&&c->srv_test_state!=2&&c->srv_test_state!=3){
            c->srv_test_state=0;
        }
        c->srvlist_probe_timer=1; /* fire immediately on next entry */
        c->srvlist_probe_idx=0;
    }
    g_ping_tick++;
    if (c->winner_timer>0) c->winner_timer--;
    if (c->taunt_timer>0)  c->taunt_timer--;
    if (c->taunt_cooldown>0) c->taunt_cooldown--;
    /* Ping — send every 120 ticks (~2s at 60fps) */
    if (c->sock != INVALID_SOCK && c->state >= CS_LOBBY_SELECT) {
        if (--c->ping_timer <= 0) {
            c->ping_timer = 120;
            char pbuf[32];
            snprintf(pbuf,sizeof(pbuf),"ping %lu",g_ping_tick);
            net_send(c->sock, pbuf);
        }
    }

    /* Auto-refresh lobby list + playerlist every ~3 seconds */
    if (c->state==CS_LOBBY_SELECT && c->sock!=INVALID_SOCK) {
        if (--c->lobby_refresh_timer <= 0) {
            c->lobby_refresh_timer = 188;
            net_send(c->sock, "listlobbies");
            net_send(c->sock, "reqleaderboard"); /* refreshes playerlist too */
        }
    }

    if (c->state!=CS_PLAYING||c->paused||c->is_spectator) return;

    /* Lock delay: if piece is grounded, count down before locking */
    if (c->lock_active) {
        /* Check if piece can still move down (e.g. it slid off the ledge) */
        Piece test = c->cur; test.y++;
        if (field_valid(&c->my_field, &test)) {
            /* No longer grounded — resume normal gravity */
            c->lock_active = 0;
            c->lock_timer = 0;
        } else {
            c->lock_timer -= 1000/60; /* subtract ~16ms per tick */
            if (c->lock_timer <= 0) {
                c->lock_active = 0;
                lock_and_spawn(c);
                /* Tell server hold is usable again */
                net_send(c->sock, "holdclear");
            }
        }
        return;
    }

    c->drop_timer--;
    if (c->drop_timer<=0) {
        c->drop_timer=c->drop_interval;
        if (!piece_move_down(&c->my_field,&c->cur)) {
            if (c->lock_delay_ms > 0) {
                /* Start lock delay */
                c->lock_active = 1;
                c->lock_timer  = c->lock_delay_ms;
            } else {
                lock_and_spawn(c);
                net_send(c->sock, "holdclear");
            }
        } else {
            proto_send_field(c->sock,c->slot,&c->my_field);
        }
    }
}

/* ============================================================
 * Main run loop
 * ============================================================ */
void client_run_gui(void) {
    ClientCtx c;
    char line[MAX_MSG_LEN];
    int n;

    srand((unsigned)time(NULL));
    net_init();
    memset(&c,0,sizeof(c));
    c.running=1; c.state=CS_SERVER_LIST; c.sock=INVALID_SOCK; c.srv_test_sock=INVALID_SOCK;
    c.drop_interval=level_to_interval(1); c.drop_timer=50; c.last_player_wins=1;
    c.sound_enabled=1;
    c.saved_music_enabled=1; c.have_saved_music=0;
    /* Keymap defaults (SDL scancodes) */
    c.km.move_left  = 80;  /* Left  */ c.km.move_right = 79;  /* Right */
    c.km.rotate_cw  = 82;  /* Up    */ c.km.rotate_ccw = 29;  /* Z     */
    c.km.soft_drop  = 81;  /* Down  */ c.km.hard_drop  = 44;  /* Space */
    c.km.hold       =  6;  /* C     */ c.km.hold2      = 225; /* LShift*/
    c.km.chat       = 43;  /* Tab   */ c.km.pause      = 62;  /* F5    */
    c.km.ready      = 21;  /* R     */
    settings_load(&c);  /* load sfx/music prefs and keymap — before sound_init */
    strncpy(c.tf_host.text,"localhost",sizeof(c.tf_host.text)-1);
    c.tf_host.len=(int)strlen(c.tf_host.text);
    strncpy(c.tf_port.text,"31457",sizeof(c.tf_port.text)-1);
    c.tf_port.len=(int)strlen(c.tf_port.text);

    /* Load server list — credentials (nick+hash) are stored per server entry */
    server_list_load(&c);
    c.state = CS_SERVER_LIST;
    /* Pre-fill nick/hash from the most recently used server entry if available */
    if (c.have_saved_creds) {
        strncpy(c.tf_nick.text, c.saved_nick, sizeof(c.tf_nick.text)-1);
        c.tf_nick.len = (int)strlen(c.tf_nick.text);
        strncpy(c.auth_pass_hash, c.saved_pw_hash, 64);
        c.tf_pass.len = 0;
        c.tf_port.focused = 1;
    } else {
        c.tf_host.focused = 1;
    }

    field_init(&c.my_field); c.my_field.level=1;
    piece_init(&c.cur,0); piece_init(&c.next,1);

    if (!sdl_init()) { net_cleanup(); return; }
    sound_init();

    /* On macOS app bundle, point music_load at Contents/Resources/
       where the .mod files are bundled. On other platforms the string
       is empty and music_load uses the current directory as usual. */
    music_set_res_prefix(sdl_get_res_dir());

    /* Apply saved music preference now that audio device is open */
    if (c.have_saved_music) music_set_enabled(c.saved_music_enabled);
    music_load("lineclash_menu.mod");

    while (c.running) {
        if (!sdl_poll_events(&c)) break;

        /* Drain network when connected */
        if (c.sock!=INVALID_SOCK && c.state!=CS_CONNECT) {
            while (1) {
                n=net_recv(c.sock,c.recv_buf,&c.recv_len,line,sizeof(line));
                if (n<0) {
                    if (c.state!=CS_CONNECT) {
                        ctx_chat(&c,"Disconnected from server",COL_INGAME_RED);
                        ctx_reset_lobby(&c);
                    }
                    sock_close(c.sock); c.sock=INVALID_SOCK;
                    c.state=CS_SERVER_LIST;
                    break;
                }
                if (n==1&&line[0]) client_handle_msg(&c,line); else break;
            }
        }

        client_tick(&c);

        if      (c.state==CS_SERVER_LIST)  sdl_render_server_list(&c);
        else if (c.state==CS_CONNECT)      sdl_render_connect(&c);
        else if (c.state==CS_AUTH)         sdl_render_connect(&c);
        else if (c.state==CS_LOBBY_SELECT) sdl_render_lobby_select(&c);
        else                               sdl_render_game(&c);

        SLEEP_MS(16);
    }

    sound_shutdown();
    sdl_shutdown();
    if (c.sock!=INVALID_SOCK) sock_close(c.sock);
    net_cleanup();
}
