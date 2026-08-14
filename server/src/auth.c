/* auth.c — SQLite3 user auth + team + persistent leaderboard */
#include "auth.h"
#include "config.h"
#include "../../shared/include/lineclash.h"
#include "../sqlite3/sqlite3.h"
/* sqlite3_changes() may not be in the bundled minimal header */
extern int sqlite3_changes(sqlite3*);
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/* ============================================================
 * SHA-256  (FIPS 180-4, public domain, no external deps)
 * ============================================================ */
typedef unsigned char      sha_u8;
typedef unsigned int       sha_u32;
typedef unsigned long long sha_u64;
static const sha_u32 SHA_K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define SR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define S0(x) (SR(x,2)^SR(x,13)^SR(x,22))
#define S1(x) (SR(x,6)^SR(x,11)^SR(x,25))
#define G0(x) (SR(x,7)^SR(x,18)^((x)>>3))
#define G1(x) (SR(x,17)^SR(x,19)^((x)>>10))
typedef struct{sha_u32 h[8];sha_u8 buf[64];sha_u64 tot;}Sha256;
static void sinit(Sha256*c){
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;c->tot=0;}
static void sblk(Sha256*c,const sha_u8*b){
    sha_u32 w[64],a,bb,cc,d,e,f,g,h,t1,t2;int i;
    for(i=0;i<16;i++) w[i]=((sha_u32)b[i*4]<<24)|((sha_u32)b[i*4+1]<<16)|((sha_u32)b[i*4+2]<<8)|b[i*4+3];
    for(i=16;i<64;i++) w[i]=G1(w[i-2])+w[i-7]+G0(w[i-15])+w[i-16];
    a=c->h[0];bb=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for(i=0;i<64;i++){t1=h+S1(e)+CH(e,f,g)+SHA_K[i]+w[i];t2=S0(a)+MJ(a,bb,cc);
        h=g;g=f;f=e;e=d+t1;d=cc;cc=bb;bb=a;a=t1+t2;}
    c->h[0]+=a;c->h[1]+=bb;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;}
static void supd(Sha256*c,const sha_u8*data,size_t len){
    size_t fill=(size_t)(c->tot%64);c->tot+=(sha_u64)len;
    if(fill){size_t r=64-fill;if(len<r){memcpy(c->buf+fill,data,len);return;}
        memcpy(c->buf+fill,data,r);sblk(c,c->buf);data+=r;len-=r;}
    while(len>=64){sblk(c,data);data+=64;len-=64;}
    if(len) memcpy(c->buf,data,len);}
static void sfin(Sha256*c,sha_u8 d[32]){
    sha_u8 p[128];sha_u64 bits=c->tot*8;size_t fill=(size_t)(c->tot%64);
    size_t pl=(fill<56)?(56-fill):(120-fill);int i;
    memset(p,0,sizeof(p));p[0]=0x80;supd(c,p,pl);
    p[0]=(sha_u8)(bits>>56);p[1]=(sha_u8)(bits>>48);p[2]=(sha_u8)(bits>>40);p[3]=(sha_u8)(bits>>32);
    p[4]=(sha_u8)(bits>>24);p[5]=(sha_u8)(bits>>16);p[6]=(sha_u8)(bits>>8);p[7]=(sha_u8)bits;
    supd(c,p,8);
    for(i=0;i<8;i++){d[i*4]=(sha_u8)(c->h[i]>>24);d[i*4+1]=(sha_u8)(c->h[i]>>16);
        d[i*4+2]=(sha_u8)(c->h[i]>>8);d[i*4+3]=(sha_u8)c->h[i];}}

void sha256_hex(const char *in,char out[AUTH_HASH_LEN]){
    Sha256 c;sha_u8 d[32];int i;
    sinit(&c);supd(&c,(const sha_u8*)in,strlen(in));sfin(&c,d);
    for(i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)d[i]);
    out[64]='\0';}

void hmac_sha256_hex(const sha_u8 key[32],const char*msg,char out[AUTH_HASH_LEN]){
    sha_u8 ip[64],op[64],inner[32],fin[32];Sha256 c;int i;
    for(i=0;i<64;i++){sha_u8 k=(i<32)?key[i]:0;ip[i]=k^0x36;op[i]=k^0x5c;}
    sinit(&c);supd(&c,ip,64);supd(&c,(const sha_u8*)msg,strlen(msg));sfin(&c,inner);
    sinit(&c);supd(&c,op,64);supd(&c,inner,32);sfin(&c,fin);
    for(i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)fin[i]);
    out[64]='\0';}

static int hex2bytes(const char*hex,sha_u8 out[32]){
    int i;if(strlen(hex)!=64)return 0;
    for(i=0;i<32;i++){unsigned int b;if(sscanf(hex+i*2,"%02x",&b)!=1)return 0;out[i]=(sha_u8)b;}
    return 1;}

void auth_make_nonce(char out[AUTH_NONCE_LEN]){
    sha_u8 buf[32];int i;
    /* Mix time + pointer + rand for nonce entropy */
    srand((unsigned)time(NULL)^(unsigned)(size_t)out^(unsigned)rand());
    for(i=0;i<32;i++) buf[i]=(sha_u8)(rand()&0xff);
    for(i=0;i<32;i++) snprintf(out+i*2,3,"%02x",(unsigned)buf[i]);
    out[64]='\0';}

/* ============================================================
 * SQLite3 — schema with team + wins columns
 * ============================================================ */
static sqlite3 *g_db=NULL;
static char g_db_path[512]="";
const char *auth_get_db_path(void){return g_db_path;}
sqlite3    *auth_get_db(void){return g_db;}

int auth_open(const char *path){
    char *err=NULL; int rc;
    if(!path||!path[0]) path=CFG_DEFAULT_DB;
    strncpy(g_db_path,path,sizeof(g_db_path)-1);
    rc=sqlite3_open(path,&g_db);
    if(rc!=SQLITE_OK){log_error("Auth: cannot open '%s': %s",path,sqlite3_errmsg(g_db));
        sqlite3_close(g_db);g_db=NULL;return 0;}
    rc=sqlite3_exec(g_db,
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "CREATE TABLE IF NOT EXISTS users("
        "  user_id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  nick       TEXT UNIQUE NOT NULL COLLATE NOCASE,"
        "  hash       TEXT NOT NULL,"
        "  team       TEXT NOT NULL DEFAULT '',"
        "  wins              INTEGER NOT NULL DEFAULT 0,"
        "  attacks           INTEGER NOT NULL DEFAULT 0,"
        "  taunts_sent       INTEGER NOT NULL DEFAULT 0,"
        "  specials_sent     INTEGER NOT NULL DEFAULT 0,"
        "  blocks_placed     INTEGER NOT NULL DEFAULT 0,"
        "  lines_cleared     INTEGER NOT NULL DEFAULT 0,"
        "  messages_sent     INTEGER NOT NULL DEFAULT 0,"
        "  kicks_from_server INTEGER NOT NULL DEFAULT 0,"
        "  kicks_from_room   INTEGER NOT NULL DEFAULT 0,"
        "  bans_received     INTEGER NOT NULL DEFAULT 0,"
        "  admin      INTEGER NOT NULL DEFAULT 0,"
        "  banned     INTEGER NOT NULL DEFAULT 0,"
        "  status     TEXT NOT NULL DEFAULT 'offline',"
        "  last_seen  TEXT NOT NULL DEFAULT '',"
        "  created_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS game_stats("
        "  stat_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id      INTEGER NOT NULL REFERENCES users(user_id),"
        "  nick         TEXT NOT NULL COLLATE NOCASE,"
        "  room_id     INTEGER NOT NULL DEFAULT 0,"
        "  game_started TEXT NOT NULL DEFAULT '',"
        "  game_ended   TEXT NOT NULL DEFAULT '',"
        "  result       TEXT NOT NULL DEFAULT 'unknown',"
        "  lines_cleared  INTEGER NOT NULL DEFAULT 0,"
        "  specials_sent  INTEGER NOT NULL DEFAULT 0,"
        "  blocks_placed  INTEGER NOT NULL DEFAULT 0,"
        "  taunts_sent    INTEGER NOT NULL DEFAULT 0,"
        "  messages_sent  INTEGER NOT NULL DEFAULT 0,"
        "  won            INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS games("
        "  game_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  room_id      INTEGER NOT NULL,"
        "  started_at   TEXT NOT NULL,"
        "  ended_at     TEXT NOT NULL DEFAULT '',"
        "  winner_nick  TEXT NOT NULL DEFAULT '',"
        "  winner_team  TEXT NOT NULL DEFAULT '',"
        "  player_count INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS lobby_settings("
        "  room_id         TEXT PRIMARY KEY NOT NULL,"
        "  no_attacks       INTEGER NOT NULL DEFAULT 0,"
        "  last_player_wins INTEGER NOT NULL DEFAULT 1"
        ");"
        "CREATE TABLE IF NOT EXISTS banned_ips("
        "  ip        TEXT PRIMARY KEY NOT NULL,"
        "  reason    TEXT NOT NULL DEFAULT '',"
        "  banned_at TEXT NOT NULL DEFAULT ''"
        ");",
        NULL,NULL,&err);
    if(rc!=SQLITE_OK){log_error("Auth: schema: %s",err);sqlite3_free(err);
        sqlite3_close(g_db);g_db=NULL;return 0;}
    /* Upgrade columns on existing DBs — ALTER TABLE ignores error if column exists */
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN team      TEXT    NOT NULL DEFAULT '';",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN wins      INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN attacks           INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN taunts_sent       INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN specials_sent     INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN blocks_placed     INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN lines_cleared     INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN messages_sent     INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN kicks_from_server INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN kicks_from_room   INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN bans_received     INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN activated       INTEGER NOT NULL DEFAULT 1;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE game_stats ADD COLUMN blocks_placed INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE game_stats ADD COLUMN taunts_sent   INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE game_stats ADD COLUMN messages_sent INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN admin     INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN banned    INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN user_id   INTEGER;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN status    TEXT    NOT NULL DEFAULT 'offline';",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE users ADD COLUMN last_seen TEXT    NOT NULL DEFAULT '';",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE banned_ips ADD COLUMN banned_by TEXT NOT NULL DEFAULT 'unknown';",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE games ADD COLUMN winner_team TEXT NOT NULL DEFAULT '';",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN topic TEXT NOT NULL DEFAULT '';",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN bot_count INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN bot_difficulty INTEGER NOT NULL DEFAULT 1;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN speedup_enabled INTEGER NOT NULL DEFAULT 1;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN no_taunts INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN garbage_system INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    sqlite3_exec(g_db,"ALTER TABLE lobby_settings ADD COLUMN garbage_targeting INTEGER NOT NULL DEFAULT 0;",NULL,NULL,NULL);
    /* Ensure game_stats and games tables exist on old DBs */
    sqlite3_exec(g_db,
        "CREATE TABLE IF NOT EXISTS game_stats("
        "  stat_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id      INTEGER NOT NULL DEFAULT 0,"
        "  nick         TEXT NOT NULL COLLATE NOCASE,"
        "  room_id     INTEGER NOT NULL DEFAULT 0,"
        "  game_started TEXT NOT NULL DEFAULT '',"
        "  game_ended   TEXT NOT NULL DEFAULT '',"
        "  result       TEXT NOT NULL DEFAULT 'unknown',"
        "  lines_cleared  INTEGER NOT NULL DEFAULT 0,"
        "  specials_sent  INTEGER NOT NULL DEFAULT 0,"
        "  blocks_placed  INTEGER NOT NULL DEFAULT 0,"
        "  taunts_sent    INTEGER NOT NULL DEFAULT 0,"
        "  messages_sent  INTEGER NOT NULL DEFAULT 0,"
        "  won            INTEGER NOT NULL DEFAULT 0"
        ");",NULL,NULL,NULL);
    sqlite3_exec(g_db,
        "CREATE TABLE IF NOT EXISTS games("
        "  game_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  room_id     INTEGER NOT NULL,"
        "  started_at   TEXT NOT NULL,"
        "  ended_at     TEXT NOT NULL DEFAULT '',"
        "  winner_nick  TEXT NOT NULL DEFAULT '',"
        "  winner_team  TEXT NOT NULL DEFAULT '',"
        "  player_count INTEGER NOT NULL DEFAULT 0"
        ");",NULL,NULL,NULL);
    log_info("Auth: opened '%s'",path);
    return 1;}

void auth_close(void){if(g_db){sqlite3_close(g_db);g_db=NULL;}}

/* ---- Timestamp helper ---- */
static void now_str(char *buf, int sz){
    time_t t=time(NULL); struct tm *tm=gmtime(&t);
    strftime(buf,sz,"%Y-%m-%dT%H:%M:%SZ",tm);
}

/* ---- User status: offline / idle / playing ---- */
void auth_set_status(const char *nick, const char *status){
    sqlite3_stmt *s=NULL; char ts[32]; now_str(ts,sizeof(ts));
    if(!g_db||!nick||!nick[0]) return;
    if(sqlite3_prepare_v2(g_db,
        "UPDATE users SET status=?, last_seen=? WHERE nick=? COLLATE NOCASE;",
        -1,&s,NULL)!=SQLITE_OK) return;
    sqlite3_bind_text(s,1,status,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,ts,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,3,nick,-1,SQLITE_STATIC);
    sqlite3_step(s); sqlite3_finalize(s);
}

/* ---- Game stats: record start of game for a player ---- */
void auth_game_stat_start(const char *nick, int room_id){
    sqlite3_stmt *s=NULL; char ts[32]; now_str(ts,sizeof(ts));
    if(!g_db||!nick||!nick[0]) return;
    if(sqlite3_prepare_v2(g_db,
        "INSERT INTO game_stats(user_id,nick,room_id,game_started)"
        " VALUES((SELECT user_id FROM users WHERE nick=? COLLATE NOCASE),?,?,?);",
        -1,&s,NULL)!=SQLITE_OK) return;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,nick,-1,SQLITE_STATIC);
    sqlite3_bind_int (s,3,room_id);
    sqlite3_bind_text(s,4,ts,-1,SQLITE_STATIC);
    sqlite3_step(s); sqlite3_finalize(s);
}

/* ---- Game stats: record end of game for a player ---- */
void auth_game_stat_end(const char *nick, int room_id, int won){
    sqlite3_stmt *s=NULL; char ts[32]; now_str(ts,sizeof(ts));
    if(!g_db||!nick||!nick[0]) return;
    if(sqlite3_prepare_v2(g_db,
        "UPDATE game_stats SET game_ended=?, result=?, won=?"
        " WHERE nick=? COLLATE NOCASE AND room_id=? AND game_ended=''"
        " ORDER BY stat_id DESC LIMIT 1;",
        -1,&s,NULL)!=SQLITE_OK) return;
    sqlite3_bind_text(s,1,ts,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,won?"win":"loss",-1,SQLITE_STATIC);
    sqlite3_bind_int (s,3,won);
    sqlite3_bind_text(s,4,nick,-1,SQLITE_STATIC);
    sqlite3_bind_int (s,5,room_id);
    sqlite3_step(s); sqlite3_finalize(s);
}

/* ---- Game record: log a full game start ---- */
int auth_game_start(int room_id, int player_count){
    sqlite3_stmt *s=NULL; char ts[32]; int game_id=-1; now_str(ts,sizeof(ts));
    if(!g_db) return -1;
    if(sqlite3_prepare_v2(g_db,
        "INSERT INTO games(room_id,started_at,player_count) VALUES(?,?,?);",
        -1,&s,NULL)!=SQLITE_OK) return -1;
    sqlite3_bind_int (s,1,room_id);
    sqlite3_bind_text(s,2,ts,-1,SQLITE_STATIC);
    sqlite3_bind_int (s,3,player_count);
    if(sqlite3_step(s)==SQLITE_DONE) game_id=(int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(s); return game_id;
}

/* ---- Game record: log game end with winner ---- */
void auth_game_end(int room_id, const char *winner_nick, const char *winner_team){
    sqlite3_stmt *s=NULL; char ts[32]; now_str(ts,sizeof(ts));
    if(!g_db) return;
    if(sqlite3_prepare_v2(g_db,
        "UPDATE games SET ended_at=?, winner_nick=?, winner_team=?"
        " WHERE room_id=? AND ended_at='' ORDER BY game_id DESC LIMIT 1;",
        -1,&s,NULL)!=SQLITE_OK) return;
    sqlite3_bind_text(s,1,ts,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,winner_nick?winner_nick:"",-1,SQLITE_STATIC);
    sqlite3_bind_text(s,3,winner_team?winner_team:"",-1,SQLITE_STATIC);
    sqlite3_bind_int (s,4,room_id);
    sqlite3_step(s); sqlite3_finalize(s);
}

int auth_count_users(void){
    sqlite3_stmt*s=NULL;int cnt=0;
    if(!g_db)return 0;
    if(sqlite3_prepare_v2(g_db,"SELECT COUNT(*) FROM users;",-1,&s,NULL)!=SQLITE_OK)return 0;
    if(sqlite3_step(s)==SQLITE_ROW){
        const char *t=(const char*)sqlite3_column_text(s,0);
        cnt=t?atoi(t):0;
    }
    sqlite3_finalize(s);return cnt;
}

static int valid_nick(const char*nick){
    size_t n=strlen(nick);int i;
    if(n<1||n>15)return 0;
    for(i=0;(size_t)i<n;i++){unsigned char ch=(unsigned char)nick[i];
        if(!isprint(ch)||ch==':'||ch==' ')return 0;}
    return 1;}

int auth_nick_exists(const char*nick){
    sqlite3_stmt*s=NULL;int rc,ex=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,"SELECT 1 FROM users WHERE nick=? COLLATE NOCASE;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);ex=(rc==SQLITE_ROW);
    sqlite3_finalize(s);return ex;}

AuthResult auth_verify_login(const char*nick,const char*nonce_hex,const char*hmac_hex){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db||!valid_nick(nick))return AUTH_ERROR;
    if(strlen(hmac_hex)!=64||strlen(nonce_hex)!=64)return AUTH_ERROR;
    rc=sqlite3_prepare_v2(g_db,"SELECT hash FROM users WHERE nick=? COLLATE NOCASE;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return AUTH_ERROR;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);
    if(rc==SQLITE_ROW){
        const char*sh=(const char*)sqlite3_column_text(s,0);
        sha_u8 key[32];char exp[AUTH_HASH_LEN];int ok=0;
        /* Corrupt hash: db_hash must be 64 hex chars */
        if(sh&&strlen(sh)!=64){
            log_info("Auth: corrupt hash for '%s' (db_hash_len=%zu) — run: dedup %s",
                     nick,strlen(sh),nick);
            sqlite3_finalize(s);
            return AUTH_ERROR;}
        if(sh&&hex2bytes(sh,key)){hmac_sha256_hex(key,nonce_hex,exp);ok=(strcmp(exp,hmac_hex)==0);}
        sqlite3_finalize(s);
        if(ok){log_info("Auth: '%s' logged in",nick);return AUTH_OK;}
        if(g_cfg.debug_auth)
            log_info("Auth: wrong password for '%s' db_hash=[%s](%zu) nonce=[%s](%zu) exp=[%s] got=[%s]",
                     nick,
                     sh?sh:"NULL", sh?strlen(sh):0,
                     nonce_hex, strlen(nonce_hex),
                     exp, hmac_hex);
        else
            log_info("Auth: wrong password for '%s' (db_hash_len=%zu)",
                     nick, sh?strlen(sh):0);
        return AUTH_WRONG_PASSWORD;}
    sqlite3_finalize(s);return AUTH_ERROR;}

AuthResult auth_register(const char*nick,const char*pw_hash_hex){
    sqlite3_stmt*s=NULL;int rc;char ts[24];
    if(!g_db||!valid_nick(nick))return AUTH_ERROR;
    if(strlen(pw_hash_hex)!=64)return AUTH_ERROR;
    {int i;for(i=0;i<64;i++)if(!isxdigit((unsigned char)pw_hash_hex[i]))return AUTH_ERROR;}
    /* Reject registration if nick already exists (case-insensitive) */
    if(auth_nick_exists(nick))return AUTH_ERROR;
    snprintf(ts,sizeof(ts),"%lld",(long long)time(NULL));
    rc=sqlite3_prepare_v2(g_db,
        "INSERT INTO users(nick,hash,team,wins,attacks,created_at) VALUES(?,?,'',0,0,?);",-1,&s,NULL);
    if(rc!=SQLITE_OK)return AUTH_ERROR;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,pw_hash_hex,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,ts,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);sqlite3_finalize(s);
    if(rc!=SQLITE_DONE){log_error("Auth: insert failed for '%s': %s",nick,sqlite3_errmsg(g_db));
        return AUTH_ERROR;}
    log_info("Auth: registered '%s'",nick);return AUTH_NEW_USER;}

/* ---- Team ---- */
void auth_get_team(const char*nick,char*out,int maxlen){
    sqlite3_stmt*s=NULL;int rc;
    out[0]='\0';if(!g_db)return;
    rc=sqlite3_prepare_v2(g_db,"SELECT team FROM users WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);
    if(rc==SQLITE_ROW){
        const char*t=(const char*)sqlite3_column_text(s,0);
        if(t) strncpy(out,t,maxlen-1);}
    sqlite3_finalize(s);}

void auth_set_team(const char*nick,const char*team){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db)return;
    rc=sqlite3_prepare_v2(g_db,"UPDATE users SET team=? WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,team,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

/* ---- Gameplay stats ---- */
static void _inc(const char*nick,const char*col,int n){
    sqlite3_stmt*st=NULL;
    char q[128]; snprintf(q,sizeof(q),"UPDATE users SET %s=%s+? WHERE nick=? COLLATE NOCASE;",col,col);
    if(!g_db||!nick||!nick[0]||n<=0)return;
    if(sqlite3_prepare_v2(g_db,q,-1,&st,NULL)!=SQLITE_OK)return;
    sqlite3_bind_int(st,1,n); sqlite3_bind_text(st,2,nick,-1,SQLITE_STATIC);
    sqlite3_step(st); sqlite3_finalize(st);
}
void auth_add_taunts(const char*nick,int n)    {_inc(nick,"taunts_sent",n);}
void auth_add_specials(const char*nick,int n)  {_inc(nick,"specials_sent",n);}
void auth_add_blocks(const char*nick,int n)    {_inc(nick,"blocks_placed",n);}
void auth_add_lines(const char*nick,int n)     {_inc(nick,"lines_cleared",n);}
void auth_add_messages(const char*nick,int n)  {_inc(nick,"messages_sent",n);}
void auth_add_kick_server(const char*nick)     {_inc(nick,"kicks_from_server",1);}
void auth_add_kick_room(const char*nick)       {_inc(nick,"kicks_from_room",1);}
void auth_add_ban(const char*nick)             {_inc(nick,"bans_received",1);}

/* ---- Wins & attacks ---- */
int auth_get_wins(const char*nick){
    sqlite3_stmt*s=NULL;int rc,w=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,"SELECT wins FROM users WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);
    if(rc==SQLITE_ROW) w=atoi((const char*)sqlite3_column_text(s,0));
    sqlite3_finalize(s);return w;}

void auth_add_win(const char*nick){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db)return;
    rc=sqlite3_prepare_v2(g_db,"UPDATE users SET wins=wins+1 WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);sqlite3_finalize(s);
    if(rc==SQLITE_DONE) log_info("Auth: win recorded for '%s'",nick);}

void auth_add_attacks(const char*nick, int count){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db||count<=0)return;
    rc=sqlite3_prepare_v2(g_db,"UPDATE users SET attacks=attacks+? WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return;
    { char cnt[12]; snprintf(cnt,sizeof(cnt),"%d",count);
      sqlite3_bind_text(s,1,cnt,-1,SQLITE_TRANSIENT); }
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

/* ---- Lobby settings persistence ---- */
void auth_get_lobby_settings(int room_id, int *no_attacks, int *last_player_wins, int *speedup_enabled) {
    sqlite3_stmt *s = NULL; int rc;
    *no_attacks = 0; *last_player_wins = 1; *speedup_enabled = 1;
    if (!g_db) return;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT no_attacks, last_player_wins, speedup_enabled FROM lobby_settings WHERE room_id=?;",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return;
    { char lid[12]; snprintf(lid,sizeof(lid),"%d",room_id);
      sqlite3_bind_text(s,1,lid,-1,SQLITE_TRANSIENT); }
    if (sqlite3_step(s) == SQLITE_ROW) {
        *no_attacks       = sqlite3_column_int(s,0);
        *last_player_wins = sqlite3_column_int(s,1);
        *speedup_enabled  = sqlite3_column_int(s,2);
    }
    sqlite3_finalize(s);
}

/* Full settings: get all persisted lobby options */
void auth_get_lobby_settings_full(int room_id,
    int *no_attacks, int *last_player_wins, int *no_taunts, int *speedup_enabled,
    int *garbage_system, int *garbage_targeting) {
    sqlite3_stmt *s = NULL; int rc;
    *no_attacks=0; *last_player_wins=1; *no_taunts=0; *speedup_enabled=1;
    *garbage_system=0; *garbage_targeting=0;
    if (!g_db) return;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT no_attacks,last_player_wins,no_taunts,speedup_enabled,garbage_system,garbage_targeting"
        " FROM lobby_settings WHERE room_id=?;", -1, &s, NULL);
    if (rc != SQLITE_OK) return;
    { char lid[12]; snprintf(lid,sizeof(lid),"%d",room_id);
      sqlite3_bind_text(s,1,lid,-1,SQLITE_TRANSIENT); }
    if (sqlite3_step(s) == SQLITE_ROW) {
        *no_attacks        = sqlite3_column_int(s,0);
        *last_player_wins  = sqlite3_column_int(s,1);
        *no_taunts         = sqlite3_column_int(s,2);
        *speedup_enabled   = sqlite3_column_int(s,3);
        *garbage_system    = sqlite3_column_int(s,4);
        *garbage_targeting = sqlite3_column_int(s,5);
    }
    sqlite3_finalize(s);
}

void auth_set_lobby_settings(int room_id, int no_attacks, int last_player_wins, int speedup_enabled) {
    sqlite3_stmt *s = NULL; int rc;
    if (!g_db) return;
    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO lobby_settings(room_id,no_attacks,last_player_wins,speedup_enabled)"
        " VALUES(?,?,?,?);",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return;
    { char lid[12]; snprintf(lid,sizeof(lid),"%d",room_id);
      sqlite3_bind_text(s,1,lid,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int(s,2,no_attacks);
      sqlite3_bind_int(s,3,last_player_wins);
      sqlite3_bind_int(s,4,speedup_enabled); }
    sqlite3_step(s); sqlite3_finalize(s);
}

/* Full settings: save all lobby options */
void auth_set_lobby_settings_full(int room_id,
    int no_attacks, int last_player_wins, int no_taunts, int speedup_enabled,
    int garbage_system, int garbage_targeting) {
    sqlite3_stmt *s = NULL; int rc;
    if (!g_db) return;
    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO lobby_settings"
        "(room_id,no_attacks,last_player_wins,no_taunts,speedup_enabled,garbage_system,garbage_targeting)"
        " VALUES(?,?,?,?,?,?,?);",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return;
    { char lid[12]; snprintf(lid,sizeof(lid),"%d",room_id);
      sqlite3_bind_text(s,1,lid,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int(s,2,no_attacks);
      sqlite3_bind_int(s,3,last_player_wins);
      sqlite3_bind_int(s,4,no_taunts);
      sqlite3_bind_int(s,5,speedup_enabled);
      sqlite3_bind_int(s,6,garbage_system);
      sqlite3_bind_int(s,7,garbage_targeting); }
    sqlite3_step(s); sqlite3_finalize(s);
}

/* ---- Individual leaderboard (wins + attacks) ---- */
int auth_get_leaderboard(LbRow *rows, int max_rows) {
    sqlite3_stmt *s = NULL; int rc, count = 0;
    if (!g_db || max_rows <= 0) return 0;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT nick, team, wins, attacks FROM users"
        " WHERE wins > 0 OR attacks > 0"
        " ORDER BY wins DESC, attacks DESC LIMIT ?;",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return 0;
    { char lim[12]; snprintf(lim,sizeof(lim),"%d",max_rows);
      sqlite3_bind_text(s,1,lim,-1,SQLITE_TRANSIENT); }
    while (sqlite3_step(s) == SQLITE_ROW && count < max_rows) {
        const char *n = (const char*)sqlite3_column_text(s,0);
        const char *t = (const char*)sqlite3_column_text(s,1);
        const char *w = (const char*)sqlite3_column_text(s,2);
        const char *a = (const char*)sqlite3_column_text(s,3);
        memset(&rows[count],0,sizeof(rows[count]));
        if (n) strncpy(rows[count].nick, n, 31);
        if (t) strncpy(rows[count].team, t, 31);
        rows[count].wins    = w ? atoi(w) : 0;
        rows[count].attacks = a ? atoi(a) : 0;
        count++;
    }
    sqlite3_finalize(s);
    return count;
}

/* ---- Team leaderboard (sum wins per team, teams only) ---- */
int auth_get_team_leaderboard(TeamLbRow *rows, int max_rows) {
    sqlite3_stmt *s = NULL; int rc, count = 0;
    if (!g_db || max_rows <= 0) return 0;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT team, SUM(wins) AS tw, COUNT(*) AS mc"
        " FROM users WHERE team != '' AND team IS NOT NULL"
        " GROUP BY team HAVING tw > 0"
        " ORDER BY tw DESC LIMIT ?;",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return 0;
    { char lim[12]; snprintf(lim,sizeof(lim),"%d",max_rows);
      sqlite3_bind_text(s,1,lim,-1,SQLITE_TRANSIENT); }
    while (sqlite3_step(s) == SQLITE_ROW && count < max_rows) {
        const char *t  = (const char*)sqlite3_column_text(s,0);
        const char *tw = (const char*)sqlite3_column_text(s,1);
        const char *mc = (const char*)sqlite3_column_text(s,2);
        memset(&rows[count],0,sizeof(rows[count]));
        if (t) strncpy(rows[count].team, t, 31);
        rows[count].wins    = tw ? atoi(tw) : 0;
        rows[count].members = mc ? atoi(mc) : 0;
        count++;
    }
    sqlite3_finalize(s);
    return count;
}


/* ---- Per-room player win counts (from game_stats) ---- */
int auth_get_room_player_wins(int room_id, LbRow *rows, int max_rows) {
    sqlite3_stmt *s = NULL; int rc, count = 0;
    if (!g_db || max_rows <= 0) return 0;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT gs.nick, u.team, COUNT(*) AS wins, 0 AS attacks"
        " FROM game_stats gs"
        " LEFT JOIN users u ON u.nick=gs.nick COLLATE NOCASE"
        " WHERE gs.room_id=? AND gs.won=1"
        " GROUP BY gs.nick ORDER BY wins DESC LIMIT ?;",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return 0;
    sqlite3_bind_int(s,1,room_id);
    sqlite3_bind_int(s,2,max_rows);
    while (sqlite3_step(s)==SQLITE_ROW && count<max_rows) {
        const char *n=(const char*)sqlite3_column_text(s,0);
        const char *t=(const char*)sqlite3_column_text(s,1);
        memset(&rows[count],0,sizeof(rows[count]));
        if(n) strncpy(rows[count].nick,n,31);
        if(t) strncpy(rows[count].team,t,31);
        rows[count].wins=(int)sqlite3_column_int(s,2);
        count++;
    }
    sqlite3_finalize(s);
    return count;
}

/* ---- Per-room team win counts (from games.winner_team) ---- */
int auth_get_room_team_wins(int room_id, TeamLbRow *rows, int max_rows) {
    sqlite3_stmt *s = NULL; int rc, count = 0;
    if (!g_db || max_rows <= 0) return 0;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT winner_team, COUNT(*) AS wins"
        " FROM games"
        " WHERE room_id=? AND winner_team != '' AND ended_at != ''"
        " GROUP BY winner_team ORDER BY wins DESC LIMIT ?;",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return 0;
    sqlite3_bind_int(s,1,room_id);
    sqlite3_bind_int(s,2,max_rows);
    while (sqlite3_step(s)==SQLITE_ROW && count<max_rows) {
        const char *t=(const char*)sqlite3_column_text(s,0);
        memset(&rows[count],0,sizeof(rows[count]));
        if(t) strncpy(rows[count].team,t,31);
        rows[count].wins=(int)sqlite3_column_int(s,1);
        count++;
    }
    sqlite3_finalize(s);
    return count;
}

/* ---- Recent games list (for API) ---- */
int auth_get_recent_games(int limit, RecentGame *out, int max_out) {
    sqlite3_stmt *s = NULL; int rc, count = 0;
    if (!g_db || max_out <= 0) return 0;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT game_id, room_id, started_at, ended_at, winner_nick, winner_team, player_count"
        " FROM games WHERE ended_at != ''"
        " ORDER BY game_id DESC LIMIT ?;",
        -1, &s, NULL);
    if (rc != SQLITE_OK) return 0;
    sqlite3_bind_int(s,1,limit<max_out?limit:max_out);
    while (sqlite3_step(s)==SQLITE_ROW && count<max_out) {
        const char *c0=(const char*)sqlite3_column_text(s,0);
        const char *c2=(const char*)sqlite3_column_text(s,2);
        const char *c3=(const char*)sqlite3_column_text(s,3);
        const char *c4=(const char*)sqlite3_column_text(s,4);
        const char *c5=(const char*)sqlite3_column_text(s,5);
        memset(&out[count],0,sizeof(out[count]));
        out[count].game_id   = c0?atoi(c0):0;
        out[count].room_id   = (int)sqlite3_column_int(s,1);
        out[count].player_count = (int)sqlite3_column_int(s,6);
        if(c2) strncpy(out[count].started_at,  c2, 31);
        if(c3) strncpy(out[count].ended_at,    c3, 31);
        if(c4) strncpy(out[count].winner_nick,  c4, 31);
        if(c5) strncpy(out[count].winner_team,  c5, 31);
        count++;
    }
    sqlite3_finalize(s);
    return count;
}
/* ---- Admin & ban management ---- */
int auth_is_admin(const char*nick){
    sqlite3_stmt*s=NULL;int rc,r=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,"SELECT admin FROM users WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW){
        int v=sqlite3_column_int(s,0);
        r=(v==1); /* only level 1 = full admin */
    }
    sqlite3_finalize(s);return r;}

int auth_get_priv_level(const char*nick){
    /* Returns the admin column value as priv level: 1=admin,2=mgr,3=normal(0→3) */
    sqlite3_stmt*s=NULL;int rc,r=3;
    if(!g_db)return 3;
    rc=sqlite3_prepare_v2(g_db,"SELECT admin FROM users WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 3;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW){
        int v=sqlite3_column_int(s,0);
        r=(v>=1&&v<=2)?v:3;
    }
    sqlite3_finalize(s);return r;}

void auth_set_admin(const char*nick,int is_admin){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db)return;
    rc=sqlite3_prepare_v2(g_db,"UPDATE users SET admin=? WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return;
    { char v[4]; snprintf(v,sizeof(v),"%d",is_admin?1:0);
      sqlite3_bind_text(s,1,v,-1,SQLITE_TRANSIENT); }
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

/* Store full privilege level (1=admin, 2=mgr, 3=normal→0) in the admin column */
void auth_set_priv_level(const char*nick,int priv){
    sqlite3_stmt*s=NULL;int rc;
    int store=(priv>=1&&priv<=2)?priv:0; /* 3=normal stored as 0 */
    if(!g_db)return;
    rc=sqlite3_prepare_v2(g_db,"UPDATE users SET admin=? WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return;
    sqlite3_bind_int(s,1,store);
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

int auth_is_activated(const char*nick){
    sqlite3_stmt*s=NULL;int rc,r=1; /* default: activated */
    if(!g_db)return 1;
    rc=sqlite3_prepare_v2(g_db,"SELECT activated FROM users WHERE nick=? COLLATE NOCASE;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 1;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW) r=sqlite3_column_int(s,0);
    sqlite3_finalize(s); return r;
}
void auth_set_activated(const char*nick,int on){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,"UPDATE users SET activated=? WHERE nick=? COLLATE NOCASE;",-1,&s,NULL)!=SQLITE_OK)return;
    sqlite3_bind_int(s,1,on?1:0);
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
}
int auth_is_banned_nick(const char*nick){
    sqlite3_stmt*s=NULL;int rc,r=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,"SELECT banned FROM users WHERE nick=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW) r=atoi((const char*)sqlite3_column_text(s,0)?:"0");
    sqlite3_finalize(s);return r;}

void auth_ban_nick(const char*nick){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,"UPDATE users SET banned=1 WHERE nick=?;",-1,&s,NULL)!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

void auth_unban_nick(const char*nick){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,"UPDATE users SET banned=0 WHERE nick=?;",-1,&s,NULL)!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

void auth_delete_user(const char*nick){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,"DELETE FROM users WHERE nick=?;",-1,&s,NULL)!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

/* IP bans */
int auth_is_banned_ip(const char*ip){
    sqlite3_stmt*s=NULL;int rc,r=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,"SELECT COUNT(*) FROM banned_ips WHERE ip=?;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,ip,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW) r=atoi((const char*)sqlite3_column_text(s,0)?:"0");
    sqlite3_finalize(s);return r>0;}

void auth_ban_ip(const char*ip,const char*reason,const char*banned_by){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO banned_ips(ip,reason,banned_at,banned_by) VALUES(?,?,?,?);",
        -1,&s,NULL)!=SQLITE_OK)return;
    char ts[24];snprintf(ts,sizeof(ts),"%lld",(long long)time(NULL));
    sqlite3_bind_text(s,1,ip,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,reason?reason:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,ts,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,banned_by?banned_by:"unknown",-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

void auth_unban_ip(const char*ip){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,"DELETE FROM banned_ips WHERE ip=?;",-1,&s,NULL)!=SQLITE_OK)return;
    sqlite3_bind_text(s,1,ip,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

int auth_list_users(char*buf,int bufsz){
    sqlite3_stmt*s=NULL;int rc,count=0,pos=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,
        "SELECT nick,team,wins,attacks,admin,banned FROM users ORDER BY nick;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    while(sqlite3_step(s)==SQLITE_ROW&&pos<bufsz-128){
        const char*nick=(const char*)sqlite3_column_text(s,0);
        const char*team=(const char*)sqlite3_column_text(s,1);
        int wins=atoi((const char*)sqlite3_column_text(s,2)?:"0");
        int atk =atoi((const char*)sqlite3_column_text(s,3)?:"0");
        int adm =atoi((const char*)sqlite3_column_text(s,4)?:"0");
        int ban =atoi((const char*)sqlite3_column_text(s,5)?:"0");
        pos+=snprintf(buf+pos,bufsz-pos,"%-16s %-12s W:%-4d Atk:%-5d%s%s\n",
                      nick?nick:"?", team&&team[0]?team:"-",
                      wins,atk,adm?" [ADMIN]":"",ban?" [BANNED]":"");
        count++;
    }
    sqlite3_finalize(s);
    return count;
}

int auth_reset_password(const char*nick,const char*pw_hash_hex){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db)return 0;
    /* Validate: must be exactly 64 lowercase hex characters */
    if(!pw_hash_hex||strlen(pw_hash_hex)!=64)return 0;
    {int _i;for(_i=0;_i<64;_i++)if(!isxdigit((unsigned char)pw_hash_hex[_i]))return 0;}
    rc=sqlite3_prepare_v2(g_db,"UPDATE users SET hash=? WHERE nick=? COLLATE NOCASE;",-1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,pw_hash_hex,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);
    int changed=sqlite3_changes(g_db);
    sqlite3_finalize(s);
    if(rc!=SQLITE_DONE)return 0;
    if(changed==0){log_info("Auth: reset_password: nick '%s' not found in DB",nick);return 0;}
    return 1;}

int auth_list_banned_ips(char*buf,int bufsz){
    sqlite3_stmt*s=NULL;int rc,count=0,pos=0;
    if(!g_db)return 0;
    rc=sqlite3_prepare_v2(g_db,
        "SELECT ip,reason,banned_at,banned_by FROM banned_ips ORDER BY banned_at DESC;",
        -1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    while(sqlite3_step(s)==SQLITE_ROW&&pos<bufsz-256){
        const char*ip  =(const char*)sqlite3_column_text(s,0);
        const char*rsn =(const char*)sqlite3_column_text(s,1);
        const char*at  =(const char*)sqlite3_column_text(s,2);
        const char*by  =(const char*)sqlite3_column_text(s,3);
        /* Convert unix timestamp to readable date */
        char date[24]="?";
        if(at&&at[0]){
            time_t t=(time_t)atoll(at);
            struct tm*tm=gmtime(&t);
            if(tm) strftime(date,sizeof(date),"%Y-%m-%d %H:%M",tm);
        }
        pos+=snprintf(buf+pos,bufsz-pos,"%-18s %-12s %-16s %s\n",
                      ip?ip:"?",
                      by&&by[0]?by:"unknown",
                      date,
                      rsn&&rsn[0]?rsn:"(no reason)");
        count++;
    }
    sqlite3_finalize(s);return count;}

void auth_set_room_topic(int room_id,const char*topic){
    sqlite3_stmt*s=NULL;
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO lobby_settings(room_id,no_attacks,last_player_wins,topic)"
        " VALUES(?,(SELECT no_attacks FROM lobby_settings WHERE room_id=? LIMIT 1),"
        "  (SELECT last_player_wins FROM lobby_settings WHERE room_id=? LIMIT 1),?);"
        ,-1,&s,NULL)!=SQLITE_OK)return;
    char id[8];snprintf(id,sizeof(id),"%d",room_id);
    sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,topic?topic:"",-1,SQLITE_TRANSIENT);
    sqlite3_step(s);sqlite3_finalize(s);}

void auth_get_room_topic(int room_id,char*out,int outsz){
    sqlite3_stmt*s=NULL; out[0]='\0';
    if(!g_db)return;
    if(sqlite3_prepare_v2(g_db,
        "SELECT topic FROM lobby_settings WHERE room_id=?;",-1,&s,NULL)!=SQLITE_OK)return;
    char id[8];snprintf(id,sizeof(id),"%d",room_id);
    sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW){
        const char*t=(const char*)sqlite3_column_text(s,0);
        if(t) strncpy(out,t,outsz-1);}
    sqlite3_finalize(s);}

/* Diagnostic: copy the raw hash bytes from DB into out, set *outlen.
 * Returns 1 if found, 0 if not found or error. */
int auth_get_raw_hash(const char*nick,char*out,int outsz,int*outlen){
    sqlite3_stmt*s=NULL;int rc;
    if(!g_db||!out||outsz<1)return 0;
    out[0]='\0'; if(outlen)*outlen=0;
    rc=sqlite3_prepare_v2(g_db,
        "SELECT hash,length(hash) FROM users WHERE nick=? COLLATE NOCASE;",
        -1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);
    if(rc==SQLITE_ROW){
        const char*h=(const char*)sqlite3_column_text(s,0);
        int l=sqlite3_column_int(s,1);
        if(outlen)*outlen=l;
        if(h){strncpy(out,h,outsz-1);out[outsz-1]='\0';}
        sqlite3_finalize(s);return 1;}
    sqlite3_finalize(s);return 0;}

/* List nicks in DB matching filter (case-insensitive LIKE, or all if filter NULL).
 * Returns number of rows found. Each line shows nick, hash length, hex bytes of nick. */
int auth_list_all_nicks(char*out,int outsz,const char*filter){
    sqlite3_stmt*s=NULL;int rc,pos=0,count=0;
    if(!g_db||!out||outsz<1){if(out&&outsz>0)out[0]='\0';return 0;}
    out[0]='\0';
    if(filter&&filter[0])
        rc=sqlite3_prepare_v2(g_db,
            "SELECT nick,length(hash) FROM users WHERE LOWER(nick)=LOWER(?) ORDER BY nick;",
            -1,&s,NULL);
    else
        rc=sqlite3_prepare_v2(g_db,
            "SELECT nick,length(hash) FROM users ORDER BY nick;",
            -1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    if(filter&&filter[0]) sqlite3_bind_text(s,1,filter,-1,SQLITE_TRANSIENT);
    while(sqlite3_step(s)==SQLITE_ROW){
        const char*n=(const char*)sqlite3_column_text(s,0);
        int l=sqlite3_column_int(s,1);
        int _ni; char _nhex[64]=""; int _nc=(int)(n?strlen(n):0);
        for(_ni=0;_ni<_nc&&_ni<20;_ni++) snprintf(_nhex+_ni*3,4,"%02x ",(unsigned char)n[_ni]);
        pos+=snprintf(out+pos,outsz-pos,"  [%s] hash=%d%s hex=[%s]\n",
                      n?n:"?",l,l==64?" ok":" CORRUPT",_nhex);
        count++;
        if(pos>=outsz-128)break;
    }
    sqlite3_finalize(s);return count;}

/* Delete corrupt (non-64-byte hash) duplicate rows for a nick (case-insensitive).
 * Returns number deleted, -1 on error. Only deletes if more than one row exists. */
int auth_delete_corrupt_dupes(const char*nick){
    sqlite3_stmt*s=NULL;int rc,total=0,deleted=0;
    if(!g_db||!nick)return -1;
    /* Count total matching rows */
    rc=sqlite3_prepare_v2(g_db,
        "SELECT COUNT(*) FROM users WHERE LOWER(nick)=LOWER(?);",
        -1,&s,NULL);
    if(rc!=SQLITE_OK)return -1;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    if(sqlite3_step(s)==SQLITE_ROW) total=sqlite3_column_int(s,0);
    sqlite3_finalize(s);
    if(total<=1)return 0; /* nothing to dedup */
    /* Delete rows where hash length != 64 */
    rc=sqlite3_prepare_v2(g_db,
        "DELETE FROM users WHERE LOWER(nick)=LOWER(?) AND length(hash)!=64;",
        -1,&s,NULL);
    if(rc!=SQLITE_OK)return -1;
    sqlite3_bind_text(s,1,nick,-1,SQLITE_TRANSIENT);
    sqlite3_step(s);
    deleted=sqlite3_changes(g_db);
    sqlite3_finalize(s);
    return deleted;}

/* Fix a corrupted hash using case-insensitive nick lookup.
 * Returns number of rows updated (should be 1 on success). */
int auth_fix_hash(const char*nick,const char*pw_hash_hex){
    sqlite3_stmt*s=NULL;int rc,changed=0;
    if(!g_db)return 0;
    if(!pw_hash_hex||strlen(pw_hash_hex)!=64)return 0;
    {int _i;for(_i=0;_i<64;_i++)if(!isxdigit((unsigned char)pw_hash_hex[_i]))return 0;}
    /* Use LIKE for case-insensitive match on nick (handles encoding quirks) */
    rc=sqlite3_prepare_v2(g_db,
        "UPDATE users SET hash=? WHERE LOWER(nick)=LOWER(?);",
        -1,&s,NULL);
    if(rc!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,pw_hash_hex,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,nick,-1,SQLITE_TRANSIENT);
    rc=sqlite3_step(s);
    changed=sqlite3_changes(g_db);
    sqlite3_finalize(s);
    if(rc!=SQLITE_DONE)return 0;
    return changed;}

void auth_get_room_bots(int room_id, int *bot_count, int *bot_difficulty) {
    *bot_count = 0; *bot_difficulty = 1;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db,
        "SELECT bot_count, bot_difficulty FROM lobby_settings WHERE room_id=?;",
        -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_int(st, 1, room_id);
    if (sqlite3_step(st) == SQLITE_ROW) {
        *bot_count      = sqlite3_column_int(st, 0);
        *bot_difficulty = sqlite3_column_int(st, 1);
    }
    sqlite3_finalize(st);
}

void auth_set_room_bots(int room_id, int bot_count, int bot_difficulty) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db,
        "INSERT INTO lobby_settings(room_id,no_attacks,last_player_wins,bot_count,bot_difficulty)"
        " VALUES(?,0,1,?,?)"
        " ON CONFLICT(room_id) DO UPDATE SET bot_count=excluded.bot_count, bot_difficulty=excluded.bot_difficulty;",
        -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_int(st, 1, room_id);
    sqlite3_bind_int(st, 2, bot_count);
    sqlite3_bind_int(st, 3, bot_difficulty);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

