/* piece.c - Piece manipulation with SRS wall kicks */
#include "../../shared/include/lineclash.h"
#include <stdlib.h>

void piece_init(Piece *p, int type) {
    p->type = type;
    p->rotation = 0;
    p->x = FIELD_WIDTH / 2 - 1;
    p->y = 0;
}

void piece_random(Piece *p, int *frequencies) {
    int total = 0, i, roll, acc = 0;
    int freq[NUM_PIECES];
    if (frequencies) {
        for (i = 0; i < NUM_PIECES; i++) { freq[i] = frequencies[i]; total += freq[i]; }
    } else {
        total = NUM_PIECES;
        for (i = 0; i < NUM_PIECES; i++) freq[i] = 1;
    }
    if (total <= 0) { piece_init(p, rand() % NUM_PIECES); return; }
    roll = rand() % total;
    for (i = 0; i < NUM_PIECES; i++) {
        acc += freq[i];
        if (roll < acc) { piece_init(p, i); return; }
    }
    piece_init(p, 0);
}

int piece_spawn(Field *f, Piece *p) {
    p->x = FIELD_WIDTH / 2 - 1;
    p->y = 0;
    p->rotation = 0;
    return field_valid(f, p);
}

/* SRS wall kick offsets for JLSTZ pieces.
 * Indexed [from_rotation][kick_index][dx,dy] — 4 kick tests after the basic. */
static const int SRS_KICKS[4][4][2] = {
    /* 0→1 */ {{ -1,0},{ -1,-1},{0,+2},{ -1,+2}},
    /* 1→2 */ {{+1,0},{+1,+1},{0,-2},{+1,-2}},
    /* 2→3 */ {{+1,0},{+1,-1},{0,+2},{+1,+2}},
    /* 3→0 */ {{ -1,0},{ -1,+1},{0,-2},{ -1,-2}},
};
/* SRS kick offsets for I piece */
static const int SRS_KICKS_I[4][4][2] = {
    /* 0→1 */ {{ -2,0},{+1,0},{ -2,+1},{+1,-2}},
    /* 1→2 */ {{ -1,0},{+2,0},{ -1,-2},{+2,+1}},
    /* 2→3 */ {{+2,0},{ -1,0},{+2,-1},{ -1,+2}},
    /* 3→0 */ {{+1,0},{ -2,0},{+1,+2},{ -2,-1}},
};

/* PIECE_CHARS: I=0 O=1 T=2 S=3 Z=4 J=5 L=6 — I piece type index */
#define PIECE_I 0
#define PIECE_O 1

static int try_rotate(Field *f, Piece *p, int new_rot) {
    Piece tmp = *p;
    int from = p->rotation & 3;
    tmp.rotation = new_rot & 3;
    /* Basic (no kick) */
    if (field_valid(f, &tmp)) { *p = tmp; return 1; }
    /* O piece — no kicks */
    if (p->type == PIECE_O) return 0;
    /* SRS kicks */
    const int (*kicks)[2] = (p->type == PIECE_I)
        ? SRS_KICKS_I[from] : SRS_KICKS[from];
    for (int k = 0; k < 4; k++) {
        tmp = *p;
        tmp.rotation = new_rot & 3;
        tmp.x += kicks[k][0];
        tmp.y -= kicks[k][1]; /* y axis inverted: +1 = up in SRS, down in grid */
        if (field_valid(f, &tmp)) { *p = tmp; return 1; }
    }
    return 0;
}

int piece_rotate_cw(Field *f, Piece *p) {
    return try_rotate(f, p, (p->rotation + 1) % 4);
}

int piece_rotate_ccw(Field *f, Piece *p) {
    return try_rotate(f, p, (p->rotation + 3) % 4);
}

int piece_move_left(Field *f, Piece *p) {
    Piece tmp = *p; tmp.x--;
    if (field_valid(f, &tmp)) { *p = tmp; return 1; }
    return 0;
}

int piece_move_right(Field *f, Piece *p) {
    Piece tmp = *p; tmp.x++;
    if (field_valid(f, &tmp)) { *p = tmp; return 1; }
    return 0;
}

int piece_move_down(Field *f, Piece *p) {
    Piece tmp = *p; tmp.y++;
    if (field_valid(f, &tmp)) { *p = tmp; return 1; }
    return 0;
}

void piece_drop(Field *f, Piece *p) {
    while (piece_move_down(f, p));
}
