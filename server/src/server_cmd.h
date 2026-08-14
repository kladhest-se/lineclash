#include <time.h>
#ifndef SERVER_CMD_H
#define SERVER_CMD_H
/* server_cmd.h -- Unified admin command system.
 * Console requires / prefix. Remote admin sends without /.
 *
 * /lsusr ["<nick>"]
 * /mkusr "<nick>" password "<pw>" privilege <1-3>
 * /chusr "<nick>" set ban 1|0
 * /chusr "<nick>" set password "<new_pw>"
 * /chusr "<nick>" set privilege <1-3>
 * /kick "<nick>" ["<reason>"]
 * /chroom <id> set specials|last_wins|taunts|topic on|off|"text"
 * /say global "<msg>"  |  /say <room_id> "<msg>"
 * /lsblock
 * /chblock set "<ip>" ["<reason>"]
 * /chblock remove "<ip>"
 * /lsroom
 * /shutdown
 * /help
 */
typedef void (*CmdOutFn)(const char *line, void *ctx);
int server_exec_command(const char *line, const char *caller,
                        CmdOutFn out, void *ctx);
#endif

/* Network stats — updated by net_send hook and server.c */
extern unsigned long g_stat_bytes_sent;
extern unsigned long g_stat_msgs_sent;
extern unsigned long g_stat_bytes_recv;
extern unsigned long g_stat_msgs_recv;
extern unsigned long g_stat_field_updates;
extern time_t        g_stat_start_time;
