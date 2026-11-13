#include "../server/src/auth.h"
#include "../server/src/config.h"
#include "lineclash.h"
#include <assert.h>

static int scalar(const char *sql) {
    sqlite3_stmt *s=NULL; int v;
    assert(sqlite3_prepare_v2(auth_get_db(),sql,-1,&s,NULL)==SQLITE_OK);
    assert(sqlite3_step(s)==SQLITE_ROW); v=sqlite3_column_int(s,0);
    sqlite3_finalize(s); return v;
}
int main(void) {
    const char *hash="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const char *nonce="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    const char *bad="0000000000000000000000000000000000000000000000000000000000000000";
    unsigned char key[32]; memset(key,0xaa,sizeof(key)); char response[65];
    assert(auth_open(":memory:"));
    char central_nick[16];
    assert(auth_central_identity("11111111111111111111111111111111","globaluser",central_nick));
    assert(!strcmp(central_nick,"@globaluser"));
    assert(auth_central_identity("11111111111111111111111111111111","globaluser",central_nick));
    assert(!auth_central_identity("22222222222222222222222222222222","globaluser",central_nick));
    assert(auth_register("@collision",hash)==AUTH_NEW_USER);
    assert(!auth_central_identity("33333333333333333333333333333333","collision",central_nick));
    sqlite3_exec(auth_get_db(),"DELETE FROM central_identities; DELETE FROM users;",NULL,NULL,NULL);
    assert(auth_register("tester",hash)==AUTH_NEW_USER);
    FILE *log=tmpfile(); assert(log); log_set_file(log);
    for(int debug=0;debug<=1;debug++) {
        g_cfg.debug_auth=debug;
        assert(auth_verify_login("tester",nonce,bad)==AUTH_WRONG_PASSWORD);
    }
    hmac_sha256_hex(key,nonce,response);
    assert(auth_verify_login("tester",nonce,response)==AUTH_OK);
    fflush(log); rewind(log); char text[4096]; size_t n=fread(text,1,sizeof(text)-1,log); text[n]=0;
    assert(!strstr(text,hash) && !strstr(text,nonce) && !strstr(text,response));
    log_set_file(NULL); fclose(log);
    assert(auth_game_start(1,2)>0); assert(auth_game_start(1,2)>0);
    auth_game_end(1,"tester","");
    assert(scalar("SELECT COUNT(*) FROM games WHERE ended_at<>'' AND winner_nick='tester'")==1);
    assert(scalar("SELECT game_id FROM games WHERE ended_at<>''")==2);
    auth_game_stat_start("tester",1); auth_game_stat_start("tester",1);
    auth_game_stat_end("tester",1,1);
    assert(scalar("SELECT COUNT(*) FROM game_stats WHERE game_ended<>'' AND won=1")==1);
    assert(scalar("SELECT stat_id FROM game_stats WHERE game_ended<>''")==2);
    assert(auth_game_start(2,2)>0); auth_game_ranked(2,1);
    auth_game_stat_start("tester",2);auth_set_status("tester","playing");
    auth_add_lines("tester",12);auth_add_specials("tester",3);auth_add_blocks("tester",50);
    auth_game_end(2,"tester","");
    assert(scalar("SELECT lines_cleared FROM game_stats WHERE room_id=2")==12);
    assert(scalar("SELECT specials_sent FROM game_stats WHERE room_id=2")==3);
    assert(scalar("SELECT blocks_placed FROM game_stats WHERE room_id=2")==50);
    assert(scalar("SELECT placement FROM game_stats WHERE room_id=2")==1);
    auth_close(); puts("Authentication and persistence regressions passed"); return 0;
}
