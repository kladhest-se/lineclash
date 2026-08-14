#ifndef AI_H
#define AI_H
#include "server_internal.h"
#include "config.h"

/* Forward declarations from server.c */
void lobby_end_game(Lobby *lb, const char *winner);
void lobby_send_field(Lobby *lb, Player *sender);

/* Difficulty constants */
#define AI_EASY   0
#define AI_MEDIUM 1
#define AI_HARD   2

/* Add a bot to a lobby. Returns slot number or -1 if full. */
int  lobby_add_bot(Lobby *lb, int difficulty);

/* Remove a bot from a lobby by slot. */
void lobby_remove_bot(Lobby *lb, int slot);

/* Called from the main server tick (~60/s). Drives all bots in the lobby. */
void lobby_tick_bots(Lobby *lb);

/* Called from lobby_start_game — initialise bot fields for a new round. */
void bot_start(Player *p, Lobby *lb);

#endif
