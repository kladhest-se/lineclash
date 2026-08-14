/* ai.c — Server-side bot AI for LineClash
 *
 * Each bot is a Player with is_bot=1 and sock=INVALID_SOCK.
 * Bots move piece-by-piece like a human: think → rotate → slide → gravity drop → lock.
 *
 * Three difficulty levels:
 *   Easy   — random column, slow/clumsy, makes mistakes
 *   Medium — height-minimising, moderate speed
 *   Hard   — hole-aware + line-clear optimised, faster but still human-paced
 */
#include "ai.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Timing helpers ────────────────────────────────────────────────────── */

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Time between pieces spawning (think pause after locking) */
static const int BOT_SPAWN_MS[3]  = { 2200, 1100, 600 };
/* Time per rotation step */
static const int BOT_ROT_MS[3]    = {  200,  120,  60 };
/* Time per horizontal move step */
static const int BOT_MOVE_MS[3]   = {  150,   80,  40 };
/* Time per gravity drop row (soft drop speed) */
static const int BOT_DROP_MS[3]   = {   80,   40,  20 };
/* Think delay before deciding (simulates human reaction) */
static const int BOT_THINK_MS[3]  = {  700,  280, 100 };
/* Random jitter added to each step (0 to N ms) */
static const int BOT_JITTER_MS[3] = {  200,   80,  20 };

/* Scale a ms delay by level — mirrors human level_to_interval curve.
 * Level 1 = full speed; each level cuts ~5% off, floor at 30% of base. */
static int bot_level_ms(int base_ms, int level) {
    if (level <= 1) return base_ms;
    float scale = 1.0f - (level - 1) * 0.05f;
    if (scale < 0.30f) scale = 0.30f;
    return (int)(base_ms * scale);
}

/* Pressure: 0.0 at level 1 (calm), 1.0 at level 15+ (max stress).
 * Used to scale mistake rate and jitter as the game speeds up. */
static float bot_pressure(int level) {
    float p = (level - 1) / 14.0f;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

/* ── Column/rotation evaluation ────────────────────────────────────────── */

/* Try placing piece p in rotation r at column col on field f.
 * Returns 1 if placement is valid, fills out_field with result. */
static int try_place(Field *f, Piece *p, int col, int rot, Field *out) {
    Piece tmp = *p;
    tmp.rotation = rot % 4;
    tmp.x = col;
    tmp.y = 0;

    /* Resolve collision from top */
    while (field_valid(f, &tmp)) tmp.y++;
    tmp.y--;
    if (!field_valid(f, &tmp)) return 0; /* can't place */

    *out = *f;
    field_lock(out, &tmp);
    return 1;
}

/* Count filled rows (cleared) */
static int count_full_rows(const Field *f) {
    int n = 0, r, c;
    for (r = 0; r < FIELD_HEIGHT; r++) {
        int full = 1;
        for (c = 0; c < FIELD_WIDTH; c++) if (!f->cells[r][c]) { full=0; break; }
        n += full;
    }
    return n;
}

/* Count holes (empty cells with a filled cell above them) */
static int count_holes(const Field *f) {
    int holes = 0, r, c;
    for (c = 0; c < FIELD_WIDTH; c++) {
        int blocked = 0;
        for (r = 0; r < FIELD_HEIGHT; r++) {
            if (f->cells[r][c]) blocked = 1;
            else if (blocked) holes++;
        }
    }
    return holes;
}

/* Sum of column heights */
static int total_height(const Field *f) {
    int total = 0, c, r;
    for (c = 0; c < FIELD_WIDTH; c++) {
        for (r = 0; r < FIELD_HEIGHT; r++) {
            if (f->cells[r][c]) { total += (FIELD_HEIGHT - r); break; }
        }
    }
    return total;
}

/* Aggregate roughness (sum of abs height differences between adjacent cols) */
static int roughness(const Field *f) {
    int heights[FIELD_WIDTH] = {0};
    int c, r;
    for (c = 0; c < FIELD_WIDTH; c++)
        for (r = 0; r < FIELD_HEIGHT; r++)
            if (f->cells[r][c]) { heights[c] = FIELD_HEIGHT - r; break; }
    int rough = 0;
    for (c = 0; c < FIELD_WIDTH-1; c++) {
        int d = heights[c] - heights[c+1];
        rough += d < 0 ? -d : d;
    }
    return rough;
}

/* Score a field placement (higher = better). Weights differ by difficulty. */
static int score_placement(const Field *result, int difficulty) {
    int lines  = count_full_rows(result);
    int holes  = count_holes(result);
    int height = total_height(result);
    int rough  = roughness(result);

    switch (difficulty) {
    case AI_EASY:
        /* Easy: prefers flat stacks and clearing lines, ignores holes.
         * Plays like a beginner — decent instincts, poor hole awareness. */
        return lines * 80 - height * 2 - rough * 2;

    case AI_MEDIUM:
        /* Medium: balanced — avoids holes, keeps stack low */
        return lines * 120 - height * 3 - holes * 15 - rough;

    case AI_HARD:
        /* Hard: aggressive line clearing, strongly avoids holes */
        return lines * 200 - height * 4 - holes * 35 - rough * 2;
    }
    return 0;
}

/* Choose best (col, rot) for current piece given difficulty.
 * Returns best score; sets *best_col and *best_rot.
 * mistake_chance: 0-100, % probability of picking a suboptimal move. */
static void ai_choose_move(Field *f, Piece *p, int difficulty,
                            int *best_col, int *best_rot, int mistake_chance) {
    /* Collect top-3 candidates for mistake simulation */
    int bs = -999999, bc = 0, br = 0;
    int bs2= -999999, bc2= 0, br2= 0;
    int bs3= -999999, bc3= 0, br3= 0;
    int max_rot = 4;

    /* Easy tries 2 rotations (more natural than 1, less than full 4) */
    if (difficulty == AI_EASY) max_rot = 2;

    for (int rot = 0; rot < max_rot; rot++) {
        for (int col = -2; col < FIELD_WIDTH + 2; col++) {
            Field result;
            Piece tmp = *p;
            if (!try_place(f, &tmp, col, rot, &result)) continue;
            field_clear_lines(&result);
            int sc = score_placement(&result, difficulty);
            if (sc > bs)       { bs3=bs2;bc3=bc2;br3=br2; bs2=bs;bc2=bc;br2=br; bs=sc;bc=col;br=rot; }
            else if (sc > bs2) { bs3=bs2;bc3=bc2;br3=br2; bs2=sc;bc2=col;br2=rot; }
            else if (sc > bs3) { bs3=sc; bc3=col;br3=rot; }
        }
    }

    /* Apply mistake based on mistake_chance (0-100%) */
    int roll = rand() % 100;
    if (roll < mistake_chance) {
        /* High pressure: sometimes pick 3rd-best or fully random */
        int severe = (mistake_chance > 50) && (rand() % 3 == 0);
        if (severe) {
            /* Panic move — random column, random rotation */
            bc = rand() % FIELD_WIDTH;
            br = rand() % max_rot;
        } else if (bs3 > -999999 && rand() % 2 == 0) {
            bc = bc3; br = br3; /* 3rd-best */
        } else if (bs2 > -999999) {
            bc = bc2; br = br2; /* 2nd-best */
        }
    }
    *best_col = bc;
    *best_rot = br;
}

/* ── Bot lifecycle ─────────────────────────────────────────────────────── */


/* Random jitter in ms */
static long jitter(int diff) {
    int j = BOT_JITTER_MS[diff];
    return j > 0 ? (rand() % j) : 0;
}


/* ── Bot personality: chat and taunt messages ─────────────────────────── */

static const char *BOT_CHAT_MSGS[] = {
    "gg",
    "nice one",
    "oh no",
    "lets go!",
    "this is fine",
    "focus...",
    "come on",
    "almost had it",
    "good game everyone",
    "i need more pieces",
    NULL
};

static const char *BOT_TAUNT_MSGS[] = {
    "a",   /* AddLine */
    "c",   /* ClearLine */
    "n",   /* NukeField */
    "q",   /* Quake */
    "g",   /* Gravity */
    NULL
};

/* Odds: 1-in-N chance per piece lock that bot sends a chat or taunt */
static const int BOT_CHAT_ODDS[3]  = { 25, 40, 60 };  /* easy chatty, hard focused */
static const int BOT_TAUNT_ODDS[3] = { 20, 30, 50 };

static void bot_maybe_chat(Player *p, Lobby *lb) {
    int d = p->bot_difficulty;
    /* Only one social action per piece */
    int do_taunt = (!lb->no_taunts) && (rand() % BOT_TAUNT_ODDS[d] == 0);
    int do_chat  = !do_taunt && (rand() % BOT_CHAT_ODDS[d] == 0);

    if (do_taunt) {
        /* Send a taunt gmsg (client-style) */
        int tidx = 0; while (BOT_TAUNT_MSGS[tidx+1]) tidx++;
        tidx = rand() % (tidx+1);
        char tmsg[MAX_MSG_LEN];
        snprintf(tmsg, sizeof(tmsg), "gmsg *** %s: *taunts*", p->nick);
        lobby_broadcast(lb, tmsg, -1);
    } else if (do_chat) {
        int cidx = 0; while (BOT_CHAT_MSGS[cidx+1]) cidx++;
        cidx = rand() % (cidx+1);
        char cmsg[MAX_MSG_LEN];
        snprintf(cmsg, sizeof(cmsg), "pline %s %s", p->nick, BOT_CHAT_MSGS[cidx]);
        lobby_broadcast(lb, cmsg, -1);
        log_info("[MSG] [Room %d] <%s> (bot) %s", lb->id, p->nick, BOT_CHAT_MSGS[cidx]);
    }
}


/* Check if game should end after a player/bot dies — mirrors server.c playerlost logic */
static void bot_check_game_end(Lobby *lb) {
    if (lb->game_state != STATE_PLAYING) return;
    int alive_count = 0;
    Player *last_alive = NULL;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected && lb->players[i].alive && !lb->players[i].spectator) {
            alive_count++;
            last_alive = &lb->players[i];
        }
    }
    if (lb->last_player_wins) {
        if (alive_count <= 1)
            lobby_end_game(lb, last_alive ? last_alive->nick : NULL);
    } else {
        if (alive_count == 0)
            lobby_end_game(lb, NULL);
    }
}

void bot_start(Player *p, Lobby *lb) {
    field_init(&p->field);
    field_init(&p->sv_field);
    p->sv_lines = 0;
    p->sv_real_level = lb->starting_level;
    p->field.level   = lb->starting_level;
    p->bot_darkness_ticks  = 0;
    p->bot_confusion_ticks = 0;
    p->sv_specials_head = p->sv_specials_tail = 0;
    p->sv_cheat_strikes = 0;
    p->sv_field_mismatch = 0;
    p->sv_piece_head = p->sv_piece_tail = p->sv_piece_sent = 0;
    p->alive = 1;
    /* Generate first piece */
    p->sv_piece_queue[0] = rand() % NUM_PIECES;
    p->sv_piece_tail = 1;
    piece_init(&p->sv_cur, p->sv_piece_queue[0]);
    piece_spawn(&p->field, &p->sv_cur);
    /* Initialise movement state */
    p->bot_next_tick_ms = now_ms();
    p->bot_phase = 0;
    p->bot_col = -1;
    p->bot_rot = 0;
    p->bot_cur_x   = p->sv_cur.x;
    p->bot_cur_rot = 0;
    p->bot_cur_y   = 0;
    (void)lb;
}

/* ── lobby_add_bot / lobby_remove_bot ─────────────────────────────────── */

int lobby_add_bot(Lobby *lb, int difficulty) {
    /* Count current players (human + bot) */
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (lb->players[i].connected) count++;
    if (count >= lb->max_players) return -1;

    /* Find free slot */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected) continue;
        Player *p = &lb->players[i];
        memset(p, 0, sizeof(*p));
        p->slot       = i + 1;
        p->connected  = 1;
        p->alive      = 0; /* not alive until game starts */
        p->ready      = 0;
        p->is_bot     = 1;
        p->bot_difficulty = difficulty;
        p->sock       = INVALID_SOCK;
        p->priv_level = 3;

        /* Generate a bot nickname */
        const char *names_easy[]   = {"Dummybot","Newbie","Slowpoke","Fumbles"};
        const char *names_medium[] = {"Stackbot","Midbot","Casual","Tetrobot"};
        const char *names_hard[]   = {"Hardbot","Zbot","Crusher","Sweeper"};
        const char **names = (difficulty==AI_EASY)  ? names_easy   :
                             (difficulty==AI_MEDIUM) ? names_medium : names_hard;
        int name_count = 4;
        snprintf(p->nick, MAX_NICK_LEN, "%s%d",
                 names[rand() % name_count], (rand() % 90) + 10);

        snprintf(p->client_type, sizeof(p->client_type), "bot");
        snprintf(p->client_platform, sizeof(p->client_platform),
                 difficulty==AI_EASY?"easy":difficulty==AI_MEDIUM?"medium":"hard");
        strncpy(p->ip, "127.0.0.1", sizeof(p->ip)-1);
        lb->player_count++; /* bots count toward room capacity */
        return p->slot;
    }
    return -1;
}

void lobby_remove_bot(Lobby *lb, int slot) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (lb->players[i].connected && lb->players[i].is_bot &&
            lb->players[i].slot == slot) {
            /* Broadcast departure */
            char buf[MAX_MSG_LEN];
            snprintf(buf, sizeof(buf), "playerleave %d", slot);
            lobby_broadcast(lb, buf, -1);
            if (lb->player_count > 0) lb->player_count--;
            memset(&lb->players[i], 0, sizeof(lb->players[i]));
            break;
        }
    }
}

/* ── Per-bot tick ─────────────────────────────────────────────────────── */
/*
 * Movement phases (p->bot_phase):
 *   0 = thinking     — wait BOT_THINK_MS then decide target col/rot
 *   1 = rotating     — apply one rotation step per tick toward bot_rot
 *   2 = moving       — slide one column per tick toward bot_col
 *   3 = dropping     — move piece down one row per tick (soft drop)
 *   4 = locking      — lock piece, clear lines, broadcast, spawn next
 */
static void bot_tick(Player *p, Lobby *lb) {
    if (!p->is_bot || !p->alive || lb->game_state != STATE_PLAYING) return;
    if (lb->paused) return;

    long now = now_ms();
    if (now < p->bot_next_tick_ms) return;

    int d = p->bot_difficulty;
    char buf[MAX_MSG_LEN];

    /* ── Phase 0: Think ─────────────────────────────────────────────── */
    if (p->bot_phase == 0) {
        /* Compute pressure-based mistake chance:
         * Easy:   10% base + up to 50% at max pressure  (10..60%)
         * Medium: 3% base + up to 25% at max pressure   (3..28%)
         * Hard:   0% base + up to 12% at max pressure   (0..12%) */
        float press = bot_pressure(p->field.level);
        int mistake_chance;
        if (d == AI_EASY)        mistake_chance = (int)(10 + press * 50);
        else if (d == AI_MEDIUM) mistake_chance = (int)( 3 + press * 25);
        else                     mistake_chance = (int)( 0 + press * 12);
        ai_choose_move(&p->field, &p->sv_cur, d, &p->bot_col, &p->bot_rot, mistake_chance);
        /* Darkness: harder to see — higher bad-placement rate + extra think delay */
        if (p->bot_darkness_ticks > 0) {
            p->bot_darkness_ticks--;
            int dark_threshold = (p->bot_darkness_ticks > 150) ? 6 : 4; /* 60% or 40% */
            if ((rand() % 10) < dark_threshold) {
                p->bot_col = rand() % FIELD_WIDTH;
                p->bot_rot = rand() % 4;
            }
            /* Extra hesitation — as if squinting to make out the field */
            p->bot_next_tick_ms = now + bot_level_ms(BOT_THINK_MS[d], p->field.level)
                                  + jitter(d) + 200 + (rand() % 300);
            p->bot_cur_x   = p->sv_cur.x;
            p->bot_cur_rot = 0;
            p->bot_cur_y   = 0;
            p->bot_phase = 1;
            return;
        }
        p->bot_cur_x   = p->sv_cur.x;   /* start from spawn x */
        p->bot_cur_rot = 0;              /* start from rotation 0 */
        p->bot_cur_y   = 0;
        p->bot_phase = 1;
        p->bot_next_tick_ms = now + bot_level_ms(BOT_THINK_MS[d], p->field.level) + jitter(d);
        return;
    }

    /* ── Phase 1: Rotate ─────────────────────────────────────────────── */
    if (p->bot_phase == 1) {
        if (p->bot_cur_rot != p->bot_rot) {
            /* Step one rotation toward target */
            /* Confusion: 25% chance to rotate the wrong way */
            if (p->bot_confusion_ticks > 0 && (rand() % 4) == 0)
                p->bot_cur_rot = (p->bot_cur_rot + 3) % 4; /* rotate CCW instead */
            else
                p->bot_cur_rot = (p->bot_cur_rot + 1) % 4;
            p->bot_next_tick_ms = now + bot_level_ms(BOT_ROT_MS[d], p->field.level) + jitter(d);
            return;
        }
        /* Done rotating — move to horizontal slide phase */
        p->bot_phase = 2;
        p->bot_next_tick_ms = now + bot_level_ms(BOT_MOVE_MS[d], p->field.level);
        return;
    }

    /* ── Phase 2: Slide horizontally ────────────────────────────────── */
    if (p->bot_phase == 2) {
        int lms = bot_level_ms(BOT_MOVE_MS[d], p->field.level);
        /* Confusion: 30% chance to move the wrong direction */
        int confused = (p->bot_confusion_ticks > 0 && (rand()%10) < 3);
        if (p->bot_confusion_ticks > 0) p->bot_confusion_ticks--;
        int want_right = (p->bot_cur_x < p->bot_col);
        int want_left  = (p->bot_cur_x > p->bot_col);
        if (confused) { int tmp=want_right; want_right=want_left; want_left=tmp; }
        if (want_right) {
            p->bot_cur_x++;
            p->bot_next_tick_ms = now + lms + jitter(d);
            return;
        } else if (want_left) {
            p->bot_cur_x--;
            p->bot_next_tick_ms = now + lms + jitter(d);
            return;
        }
        /* At target column — start dropping */
        p->bot_phase = 3;
        p->bot_next_tick_ms = now + bot_level_ms(BOT_DROP_MS[d], p->field.level);
        return;
    }

    /* ── Phase 3: Drop one row ───────────────────────────────────────── */
    if (p->bot_phase == 3) {
        /* Build the piece at current position to test if we can move down */
        Piece test = p->sv_cur;
        test.rotation = p->bot_cur_rot % 4;
        test.x = p->bot_cur_x;
        test.y = p->bot_cur_y + 1;
        if (field_valid(&p->field, &test)) {
            p->bot_cur_y++;
            /* Under pressure, occasionally hesitate mid-drop */
            float prs = bot_pressure(p->field.level);
            int extra = (rand() % 100 < (int)(prs * 30)) ? (50 + rand() % 150) : 0;
            p->bot_next_tick_ms = now + bot_level_ms(BOT_DROP_MS[d], p->field.level) + jitter(d)/2 + extra;
            return;
        }
        /* Can't drop further — lock it */
        p->bot_phase = 4;
        p->bot_next_tick_ms = now + 30; /* tiny delay before lock */
        return;
    }

    /* ── Phase 4: Lock piece and broadcast ──────────────────────────── */
    {
        Piece placed = p->sv_cur;
        placed.rotation = p->bot_cur_rot % 4;
        placed.x = p->bot_cur_x;
        placed.y = p->bot_cur_y;

        if (!field_valid(&p->field, &placed)) {
            /* Topped out */
            p->alive = 0;
            snprintf(buf, sizeof(buf), "playerlost %d", p->slot);
            lobby_broadcast(lb, buf, p->slot);
            log_info("Bot %s topped out (room %d)", p->nick, lb->id);
            bot_check_game_end(lb);
            return;
        }

        Field new_field = p->field;
        field_lock(&new_field, &placed);
        int cleared = field_clear_lines(&new_field);
        p->sv_lines += cleared;
        new_field.lines = p->sv_lines;
        if (cleared > 0) g_stat_lines_cleared += cleared;
        /* Detect perfect clear */
        int is_perfect_bot = (cleared > 0);
        for (int _pc=0; _pc<FIELD_WIDTH&&is_perfect_bot; _pc++)
            for (int _pr=0; _pr<FIELD_HEIGHT&&is_perfect_bot; _pr++)
                if (new_field.cells[_pr][_pc]) is_perfect_bot=0;

        /* Level & score — use same config-driven logic as human lineclear handler */
        if (cleared > 0 && lb->lines_per_level > 0) {
            int prev_trg = (p->sv_lines - cleared) / lb->lines_per_level;
            int curr_trg = p->sv_lines / lb->lines_per_level;
            int lvl_ups  = curr_trg - prev_trg;
            if (lvl_ups > 0) {
                p->sv_real_level += lvl_ups * lb->level_increment;

                int active_level;
                if (lb->average_levels) {
                    int sum = 0, count = 0, ai;
                    for (ai = 0; ai < MAX_PLAYERS; ai++) {
                        Player *ap = &lb->players[ai];
                        if (ap->connected && ap->alive && !ap->spectator) {
                            sum += ap->sv_real_level; count++;
                        }
                    }
                    active_level = count > 0 ? (sum + count - 1) / count : p->sv_real_level;
                } else {
                    active_level = p->sv_real_level;
                }

                /* Push active level to all eligible players */
                for (int li2 = 0; li2 < MAX_PLAYERS; li2++) {
                    Player *op = &lb->players[li2];
                    if (!op->connected || op->spectator || !op->alive) continue;
                    if (!lb->average_levels && op->slot != p->slot) continue;
                    if (op->field.level >= active_level) continue;
                    op->field.level = active_level;
                    snprintf(buf, sizeof(buf), "lvl %d %d", op->slot, active_level);
                    if (!op->is_bot) net_send(op->sock, buf);
                    lobby_broadcast(lb, buf, op->slot);
                }
            }
        }
        new_field.score += cleared * cleared * 100 * p->field.level;
        new_field.level = p->field.level;
        p->field = new_field;

        /* Broadcast field */
        {
            char encoded[FIELD_WIDTH * FIELD_HEIGHT + 8];
            field_encode(&p->field, encoded, sizeof(encoded));
            snprintf(buf, sizeof(buf), "f %d %s", p->slot, encoded);
            lobby_broadcast(lb, buf, -1);
        }

        /* ── Specials: award using lines_per_special config (same as humans) ── */
        if (!lb->no_attacks && cleared > 0 && lb->lines_per_special > 0 && lb->specials_added > 0) {
            int prev_trg = (p->sv_lines - cleared) / lb->lines_per_special;
            int curr_trg = p->sv_lines / lb->lines_per_special;
            int to_award = (curr_trg - prev_trg) * lb->specials_added;
            int held = (p->sv_specials_tail - p->sv_specials_head + 64) % 64;
            if (held + to_award > lb->specials_capacity) to_award = lb->specials_capacity - held;
            for (int gi = 0; gi < to_award; gi++) {
                if ((p->sv_specials_tail - p->sv_specials_head + 64) % 64 >= 62) break;
                uint8 sp = (uint8)((rand() % NUM_SPECIALS) + 1);
                p->sv_specials[p->sv_specials_tail % 64] = sp;
                p->sv_specials_tail = (p->sv_specials_tail + 1) % 64;
            }
        }

        /* ── Garbage attacks from line clears (mirrors human lineclear handler) ── */
        if (!lb->no_attacks && cleared >= 1) {
            int garbage = 0;
            int is_power = (cleared == 4);

            if (lb->garbage_system == 1) {
                /* Guideline: B2B + combos + perfect clear */
                static const int base_tbl[5] = {0, 0, 1, 2, 4};
                garbage = base_tbl[cleared];
                if (is_perfect_bot && g_cfg.perfect_clear_bonus > 0) {
                    garbage = g_cfg.perfect_clear_bonus;
                    char pcmsg[MAX_MSG_LEN];
                    snprintf(pcmsg,sizeof(pcmsg),"gmsg *** %s (bot): PERFECT CLEAR! (+%d lines!)",
                             p->nick, garbage);
                    lobby_broadcast(lb, pcmsg, -1);
                    p->sv_b2b = 0; p->sv_combo = 0;
                    goto send_garbage_bot;
                }
                if (is_power && p->sv_b2b && garbage > 0) garbage += g_cfg.btb_bonus;
                if (p->sv_combo > 0) {
                    int cb = (p->sv_combo - 1) / 2;
                    if (cb > g_cfg.combo_cap) cb = g_cfg.combo_cap;
                    if (garbage > 0 || p->sv_combo >= 2) garbage += cb;
                }
                if (p->sv_combo >= 2 || (is_power && p->sv_b2b)) {
                    char gmsg[MAX_MSG_LEN];
                    if (is_power && p->sv_b2b && p->sv_combo >= 2)
                        snprintf(gmsg,sizeof(gmsg),"gmsg *** %s: B2B falling blocks + %d-combo! (+%d lines)",
                                 p->nick, p->sv_combo, garbage);
                    else if (is_power && p->sv_b2b)
                        snprintf(gmsg,sizeof(gmsg),"gmsg *** %s: Back-to-Back falling blocks! (+%d lines)",
                                 p->nick, garbage);
                    else
                        snprintf(gmsg,sizeof(gmsg),"gmsg *** %s: %d-combo! (+%d lines)",
                                 p->nick, p->sv_combo, garbage);
                    lobby_broadcast(lb, gmsg, -1);
                }
                p->sv_b2b = is_power ? 1 : 0;
                p->sv_combo++;
            } else {
                /* Classic LineClash */
                static const int cls_tbl[5] = {0, 0, 1, 2, 4};
                garbage = cls_tbl[cleared];
                p->sv_combo = 0; p->sv_b2b = 0;
            }

            send_garbage_bot:;
            if (garbage > 0) {
                int targets[MAX_PLAYERS], nt = 0;
                for (int ti = 0; ti < MAX_PLAYERS; ti++) {
                    Player *tp = &lb->players[ti];
                    if (tp->connected && tp->alive && !tp->spectator && tp->slot != p->slot)
                        targets[nt++] = ti;
                }
                if (nt > 0) {
                    int send_to[MAX_PLAYERS], ns = 0;
                    if (lb->garbage_targeting == 1) {
                        send_to[0] = targets[rand() % nt]; ns = 1;
                    } else if (lb->garbage_targeting == 2) {
                        static int bot_focus[MAX_PLAYERS] = {0};
                        int fi = bot_focus[p->slot-1] % nt;
                        send_to[0] = targets[fi]; ns = 1;
                        bot_focus[p->slot-1] = (fi + 1) % nt;
                    } else {
                        for (int si=0;si<nt;si++) { send_to[si]=targets[si]; } ns=nt;
                    }
                    for (int si = 0; si < ns; si++) {
                        Player *tgt = &lb->players[send_to[si]];
                        char sbuf[48];
                        for (int gl = 0; gl < garbage; gl++) {
                            snprintf(sbuf,sizeof(sbuf),"sb %d a %d",tgt->slot,p->slot);
                            lobby_broadcast(lb,sbuf,-1);
                        }
                        field_add_garbage_styled(&tgt->field, garbage, g_cfg.garbage_hole_style);
                        lobby_send_field(lb, tgt);
                        if (!tgt->is_bot) {
                            char enc[FIELD_WIDTH*FIELD_HEIGHT+8], fm[MAX_MSG_LEN];
                            field_encode(&tgt->field,enc,sizeof(enc));
                            snprintf(fm,sizeof(fm),"f %d %s",tgt->slot,enc);
                            net_send(tgt->sock,fm);
                        }
                    }
                }
            } else if (lb->garbage_system == 1 && cleared < 2) {
                /* Single clear in guideline breaks combo */
                p->sv_combo = 0; p->sv_b2b = 0;
            }
        } else {
            /* No attacks or 0 lines — reset combo in both modes */
            p->sv_combo = 0; p->sv_b2b = 0;
        }

        /* ── Fire specials: hard always, medium 50%, easy never ── */
        if (!lb->no_attacks && p->sv_specials_head != p->sv_specials_tail) {
            int should_fire = (p->bot_difficulty == AI_HARD) ||
                              (p->bot_difficulty == AI_MEDIUM && (rand() % 2) == 0);
            if (should_fire) {
                /* Build alive target list */
                int targets[MAX_PLAYERS], nt = 0;
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (lb->players[i].connected && lb->players[i].alive &&
                        !lb->players[i].spectator && lb->players[i].slot != p->slot)
                        targets[nt++] = i;
                }
                /* Pick a target */
                if (nt > 0) {
                    uint8 sp = p->sv_specials[p->sv_specials_head % 64];
                    p->sv_specials_head = (p->sv_specials_head + 1) % 64;
                    /* Count own filled rows to judge "messy" (>=12 = messy) */
                    int own_filled = 0;
                    for (int ry2=0;ry2<FIELD_HEIGHT;ry2++)
                        for (int rx2=0;rx2<FIELD_WIDTH;rx2++)
                            if(p->field.cells[ry2][rx2]) own_filled++;
                    int own_messy = (own_filled > FIELD_WIDTH * 12);
                    /* Prefer NUKEFIELD on own field when very messy */
                    if (sp == SP_NUKEFIELD && own_messy) {
                        /* Fire nuke on self — clears the mess */
                        char spchar = SPECIAL_CHARS[sp - 1];
                        snprintf(buf, sizeof(buf), "sb %d %c %d", p->slot, spchar, p->slot);
                        lobby_broadcast(lb, buf, -1);
                        field_nuke(&p->field);
                        lobby_send_field(lb, p);
                        goto done_special_bot;
                    }
                    /* Self-beneficial specials go to self */
                    int self_only = (sp == SP_CLEARLINE || sp == SP_CLEARSPECIAL || sp == SP_IMMUNITY);
                    int tslot = p->slot; /* default self */
                    if (!self_only && nt > 0) {
                        tslot = lb->players[targets[rand() % nt]].slot;
                    }
                    char spchar = SPECIAL_CHARS[sp - 1];
                    snprintf(buf, sizeof(buf), "sb %d %c %d", tslot, spchar, p->slot);
                    lobby_broadcast(lb, buf, -1);
                    /* Apply to target field */
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        Player *tgt = &lb->players[i];
                        if (!tgt->connected || tgt->slot != tslot) continue;
                        switch (sp) {
                            case SP_ADDLINE:      field_add_garbage(&tgt->field, 1); break;
                            case SP_CLEARLINE:    field_clear_line_bottom(&tgt->field); break;
                            case SP_CLEARSPECIAL: field_clear_specials(&tgt->field); break;
                            case SP_RANDOMCLEAR:  field_random_clear(&tgt->field); break;
                            case SP_BLOCKBOMB:    field_block_bomb(&tgt->field, NULL); break;
                            case SP_CLEARCOLUMN:  field_clear_column(&tgt->field); break;
                            case SP_NUKEFIELD:    field_nuke(&tgt->field); break;
                            case SP_GRAVITY:      field_gravity(&tgt->field); break;
                            case SP_QUAKEFIELD:   field_quake(&tgt->field); break;
                            case SP_SWITCHFIELD: {
                                /* swap with a random opponent */
                                if (nt > 0) {
                                    Player *other = &lb->players[targets[rand()%nt]];
                                    Field tmp = tgt->field; tgt->field = other->field; other->field = tmp;
                                    lobby_send_field(lb, other);
                                    if (!other->is_bot) {
                                        char enc3[FIELD_WIDTH*FIELD_HEIGHT+8], fm3[MAX_MSG_LEN];
                                        field_encode(&other->field,enc3,sizeof(enc3));
                                        snprintf(fm3,sizeof(fm3),"f %d %s",other->slot,enc3);
                                        net_send(other->sock,fm3);
                                    }
                                }
                                break;
                            }
                            default: break;
                        }
                        lobby_send_field(lb, tgt);
                        if (!tgt->is_bot) {
                            char enc2[FIELD_WIDTH*FIELD_HEIGHT+8], fb[MAX_MSG_LEN];
                            field_encode(&tgt->field,enc2,sizeof(enc2));
                            snprintf(fb,sizeof(fb),"f %d %s",tgt->slot,enc2);
                            net_send(tgt->sock,fb);
                        }
                        break;
                    }
                }
                done_special_bot:;
            }
        }

        /* Spawn next piece */
        int next_type = rand() % NUM_PIECES;
        piece_init(&p->sv_cur, next_type);
        piece_spawn(&p->field, &p->sv_cur);

        if (!field_valid(&p->field, &p->sv_cur)) {
            p->alive = 0;
            snprintf(buf, sizeof(buf), "playerlost %d", p->slot);
            lobby_broadcast(lb, buf, p->slot);
            log_info("Bot %s topped out (room %d)", p->nick, lb->id);
            bot_check_game_end(lb);
            return;
        }

        /* Occasional chat or taunt */
        bot_maybe_chat(p, lb);

        /* Reset for next piece */
        p->bot_phase = 0;
        p->bot_col   = -1;
        p->bot_cur_x   = p->sv_cur.x;
        p->bot_cur_rot = 0;
        p->bot_cur_y   = 0;
        /* Pause between pieces — varies by difficulty with jitter */
        p->bot_next_tick_ms = now + bot_level_ms(BOT_SPAWN_MS[d], p->field.level) + jitter(d)*2;
    }
}


/* ── lobby_tick_bots ───────────────────────────────────────────────────── */

void lobby_tick_bots(Lobby *lb) {
    if (lb->game_state != STATE_PLAYING) return;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &lb->players[i];
        if (p->connected && p->is_bot && p->alive)
            bot_tick(p, lb);
    }
}
