/* protocol.c - LineClash protocol helpers */
#include "../../shared/include/lineclash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Parse a received line into command + arguments.
 * Returns number of tokens (cmd + args).
 * ============================================================ */
int proto_parse(const char *line, char *cmd, char args[][MAX_MSG_LEN], int max_args) {
    /* Static to avoid large stack allocation (was 256KB per call) */
    static char all[MAX_MSG_LEN + 1][MAX_MSG_LEN];
    int n, i;

    /* Split on space */
    n = str_split(line, ' ', all, max_args + 1);
    if (n == 0) { cmd[0] = '\0'; return 0; }

    /* cmd buffer is caller-defined (typically 64 bytes) — copy only the string */
    { int clen=(int)strlen(all[0]); if(clen>63)clen=63;
      memcpy(cmd, all[0], clen); cmd[clen]='\0'; }

    for (i = 0; i < n - 1 && i < max_args; i++) {
        int alen=(int)strlen(all[i+1]); if(alen>MAX_MSG_LEN-1)alen=MAX_MSG_LEN-1;
        memcpy(args[i], all[i+1], alen); args[i][alen]='\0';
    }
    return n;
}

/* ============================================================
 * Hello / join
 * ============================================================ */
void proto_send_join(sock_t s, const char *nick, const char *team, int slot) { (void)slot;
    char msg[MAX_MSG_LEN];
    snprintf(msg, sizeof(msg), "lineclash_join %s %s", nick, (team && team[0]) ? team : "-");
    net_send(s, msg);
}

/* ============================================================
 * Field update
 * ============================================================ */
void proto_send_field(sock_t s, int slot, Field *f) {
    char encoded[FIELD_WIDTH * FIELD_HEIGHT + 8];
    char msg[MAX_MSG_LEN];
    field_encode(f, encoded, sizeof(encoded));
    snprintf(msg, sizeof(msg), "f %d %s", slot, encoded);
    net_send(s, msg);
}

/* ============================================================
 * Special block
 * ============================================================ */
void proto_send_special(sock_t s, int from_slot, uint8 sp, int to_slot) {
    char msg[MAX_MSG_LEN];
    snprintf(msg, sizeof(msg), "sb %d %c %d", to_slot, SPECIAL_CHARS[sp - 1], from_slot);
    net_send(s, msg);
}

/* ============================================================
 * Chat
 * ============================================================ */
void proto_send_chat(sock_t s, int slot, const char *text) {
    char msg[MAX_MSG_LEN];
    snprintf(msg, sizeof(msg), "pline %d %s", slot, text);
    net_send(s, msg);
}

void proto_send_gmsg(sock_t s, const char *text) {
    char msg[MAX_MSG_LEN];
    snprintf(msg, sizeof(msg), "gmsg %s", text);
    net_send(s, msg);
}

/* ============================================================
 * Level update
 * ============================================================ */
void proto_send_level(sock_t s, int slot, int level) {
    char msg[MAX_MSG_LEN];
    snprintf(msg, sizeof(msg), "lvl %d %d", slot, level);
    net_send(s, msg);
}

/* ============================================================
 * Game over
 * ============================================================ */
void proto_send_gameover(sock_t s, int slot) {
    char msg[MAX_MSG_LEN];
    snprintf(msg, sizeof(msg), "playerlost %d", slot);
    net_send(s, msg);
}
