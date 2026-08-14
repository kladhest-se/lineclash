/* field.c - Field and piece data */
#include "../../shared/include/lineclash.h"
#include <stdlib.h>

/* ============================================================
 * Piece shape data
 * Each piece has 4 rotations; each rotation has 4 cells (dx,dy) offsets.
 * ============================================================ */
const PieceShape PIECES[NUM_PIECES][4] = {
    /* 0: I piece */
    {
        {{{-1,0},{0,0},{1,0},{2,0}}},
        {{{0,-1},{0,0},{0,1},{0,2}}},
        {{{-1,0},{0,0},{1,0},{2,0}}},
        {{{0,-1},{0,0},{0,1},{0,2}}}
    },
    /* 1: O piece */
    {
        {{{0,0},{1,0},{0,1},{1,1}}},
        {{{0,0},{1,0},{0,1},{1,1}}},
        {{{0,0},{1,0},{0,1},{1,1}}},
        {{{0,0},{1,0},{0,1},{1,1}}}
    },
    /* 2: T piece */
    {
        {{{-1,0},{0,0},{1,0},{0,1}}},
        {{{0,-1},{0,0},{0,1},{-1,0}}},
        {{{-1,0},{0,0},{1,0},{0,-1}}},
        {{{0,-1},{0,0},{0,1},{1,0}}}
    },
    /* 3: S piece */
    {
        {{{0,0},{1,0},{-1,1},{0,1}}},
        {{{0,-1},{0,0},{1,0},{1,1}}},
        {{{0,0},{1,0},{-1,1},{0,1}}},
        {{{0,-1},{0,0},{1,0},{1,1}}}
    },
    /* 4: Z piece */
    {
        {{{-1,0},{0,0},{0,1},{1,1}}},
        {{{0,0},{0,1},{-1,1},{-1,2}}},
        {{{-1,0},{0,0},{0,1},{1,1}}},
        {{{0,0},{0,1},{-1,1},{-1,2}}}
    },
    /* 5: J piece */
    {
        {{{-1,0},{0,0},{1,0},{-1,1}}},
        {{{0,-1},{0,0},{0,1},{1,1}}},
        {{{-1,0},{0,0},{1,0},{1,-1}}},
        {{{0,-1},{0,0},{0,1},{-1,-1}}}
    },
    /* 6: L piece */
    {
        {{{-1,0},{0,0},{1,0},{1,1}}},
        {{{0,-1},{0,0},{0,1},{-1,1}}},
        {{{-1,0},{0,0},{1,0},{-1,-1}}},
        {{{0,-1},{0,0},{0,1},{1,-1}}}
    }
};

/* Display chars for each piece type (colors handled by render) */
const char PIECE_CHARS[NUM_PIECES] = { 'I','O','T','S','Z','J','L' };

/* Special block chars — one per SP_ define, in order 1-14 */
const char SPECIAL_CHARS[NUM_SPECIALS + 1] = "acbroqgsnivmdf";
/* a=addline  c=clearline  b=clearspecial  r=randomclear  o=blockbomb
   q=quake    g=gravity    s=switchfield   n=nukefield    i=immunity
   v=clearcolumn  m=mutate  d=darkness  f=confusion */

/* ============================================================
 * Field operations
 * ============================================================ */
void field_init(Field *f) {
    memset(f->cells, 0, sizeof(f->cells));
    f->score = 0;
    f->level = 1;
    f->lines = 0;
    f->special_count = 0;
    f->special_head = 0;
    f->special_tail = 0;
    memset(f->specials, 0, sizeof(f->specials));
}

int field_valid(Field *f, Piece *p) {
    const PieceShape *shape = &PIECES[p->type][p->rotation];
    int i, nx, ny;
    for (i = 0; i < 4; i++) {
        nx = p->x + shape->c[i].dx;
        ny = p->y + shape->c[i].dy;
        if (nx < 0 || nx >= FIELD_WIDTH) return 0;
        if (ny >= FIELD_HEIGHT) return 0;
        if (ny >= 0 && f->cells[ny][nx] != CELL_EMPTY) return 0;
    }
    return 1;
}

void field_lock(Field *f, Piece *p) {
    const PieceShape *shape = &PIECES[p->type][p->rotation];
    int i, nx, ny;
    /* piece type + 1 = cell value (1-7) */
    uint8 val = (uint8)(p->type + 1);
    for (i = 0; i < 4; i++) {
        nx = p->x + shape->c[i].dx;
        ny = p->y + shape->c[i].dy;
        if (ny >= 0 && ny < FIELD_HEIGHT && nx >= 0 && nx < FIELD_WIDTH) {
            f->cells[ny][nx] = val;
        }
    }
}

int field_clear_lines(Field *f) {
    int y, x, cleared = 0;
    int dst = FIELD_HEIGHT - 1;

    for (y = FIELD_HEIGHT - 1; y >= 0; y--) {
        int full = 1;
        for (x = 0; x < FIELD_WIDTH; x++) {
            if (f->cells[y][x] == CELL_EMPTY) { full = 0; break; }
        }
        if (!full) {
            if (dst != y) {
                memcpy(f->cells[dst], f->cells[y], FIELD_WIDTH);
            }
            dst--;
        } else {
            cleared++;
        }
    }
    /* Clear top rows */
    for (y = dst; y >= 0; y--) {
        memset(f->cells[y], 0, FIELD_WIDTH);
    }
    return cleared;
}

void field_add_garbage(Field *f, int lines) {
    int y, x, hole;
    if (lines <= 0) return;
    /* Shift field up */
    if (lines >= FIELD_HEIGHT) {
        memset(f->cells, 0, sizeof(f->cells));
        lines = FIELD_HEIGHT;
    } else {
        memmove(f->cells[0], f->cells[lines],
                (FIELD_HEIGHT - lines) * FIELD_WIDTH);
    }
    /* Fill bottom lines with garbage — semi-random hole (changes ~1/3 of rows) */
    hole = rand() % FIELD_WIDTH;
    for (y = FIELD_HEIGHT - lines; y < FIELD_HEIGHT; y++) {
        for (x = 0; x < FIELD_WIDTH; x++) {
            f->cells[y][x] = (x == hole) ? CELL_EMPTY : CELL_GARBAGE;
        }
        if (rand() % 3 == 0) hole = rand() % FIELD_WIDTH;
    }
}

/* hole_style: 0=random per line, 1=same column for whole attack, 2=fixed column 4 */
void field_add_garbage_styled(Field *f, int lines, int hole_style) {
    int y, x, hole;
    if (lines <= 0) return;
    if (lines >= FIELD_HEIGHT) {
        memset(f->cells, 0, sizeof(f->cells));
        lines = FIELD_HEIGHT;
    } else {
        memmove(f->cells[0], f->cells[lines],
                (FIELD_HEIGHT - lines) * FIELD_WIDTH);
    }
    hole = (hole_style == 2) ? 4 : rand() % FIELD_WIDTH;
    for (y = FIELD_HEIGHT - lines; y < FIELD_HEIGHT; y++) {
        for (x = 0; x < FIELD_WIDTH; x++)
            f->cells[y][x] = (x == hole) ? CELL_EMPTY : CELL_GARBAGE;
        if (hole_style == 0) hole = rand() % FIELD_WIDTH; /* new hole each row */
        /* style 1: hole stays same for whole attack; style 2: always col 4 */
    }
}

void field_nuke(Field *f) {
    memset(f->cells, 0, sizeof(f->cells));
}

void field_random_clear(Field *f) {
    int count = (FIELD_WIDTH * FIELD_HEIGHT) / 10;
    int i;
    for (i = 0; i < count; i++) {
        int x = rand() % FIELD_WIDTH;
        int y = rand() % FIELD_HEIGHT;
        f->cells[y][x] = CELL_EMPTY;
    }
}

void field_quake(Field *f) {
    /* Randomly shift each row left or right by up to 3 */
    int y;
    uint8 tmp[FIELD_WIDTH];
    for (y = 0; y < FIELD_HEIGHT; y++) {
        int shift = (rand() % 7) - 3; /* -3 to +3 */
        int x;
        memset(tmp, 0, FIELD_WIDTH);
        for (x = 0; x < FIELD_WIDTH; x++) {
            int nx = x + shift;
            if (nx >= 0 && nx < FIELD_WIDTH) {
                tmp[nx] = f->cells[y][x];
            }
        }
        memcpy(f->cells[y], tmp, FIELD_WIDTH);
    }
}

void field_gravity(Field *f) {
    /* All cells fall to bottom */
    int x, y, dst;
    for (x = 0; x < FIELD_WIDTH; x++) {
        dst = FIELD_HEIGHT - 1;
        for (y = FIELD_HEIGHT - 1; y >= 0; y--) {
            if (f->cells[y][x] != CELL_EMPTY) {
                f->cells[dst][x] = f->cells[y][x];
                if (dst != y) f->cells[y][x] = CELL_EMPTY;
                dst--;
            }
        }
        while (dst >= 0) {
            f->cells[dst][x] = CELL_EMPTY;
            dst--;
        }
    }
}

void field_clear_specials(Field *f) {
    /* Remove all special cells from field (replace with normal blocks) */
    int x, y;
    for (y = 0; y < FIELD_HEIGHT; y++) {
        for (x = 0; x < FIELD_WIDTH; x++) {
            if (f->cells[y][x] > NUM_PIECES) {
                /* Replace special with normal garbage */
                f->cells[y][x] = CELL_GARBAGE;
            }
        }
    }
}

void field_block_bomb(Field *f, Piece *p) {
    /* Clear a 5x5 area around piece position (or random spot if no piece) */
    int cx, cy, dx, dy, nx, ny;
    if (p) {
        cx = p->x; cy = p->y;
    } else {
        /* Server-side: pick a random occupied cell as centre */
        cx = rand() % FIELD_WIDTH;
        cy = FIELD_HEIGHT / 2 + rand() % (FIELD_HEIGHT / 2);
    }
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            nx = cx + dx; ny = cy + dy;
            if (nx >= 0 && nx < FIELD_WIDTH && ny >= 0 && ny < FIELD_HEIGHT)
                f->cells[ny][nx] = CELL_EMPTY;
        }
    }
}

/* Clear bottom-most occupied line (ClearLine special) */
void field_clear_line_bottom(Field *f) {
    int y, x;
    for (y = FIELD_HEIGHT - 1; y >= 0; y--) {
        for (x = 0; x < FIELD_WIDTH; x++) {
            if (f->cells[y][x] != CELL_EMPTY) {
                /* Clear this row and shift everything down */
                memmove(f->cells[1], f->cells[0], y * FIELD_WIDTH);
                memset(f->cells[0], 0, FIELD_WIDTH);
                return;
            }
        }
    }
}

/* Delete a random column (ClearColumn special) */
void field_clear_column(Field *f) {
    int col = rand() % FIELD_WIDTH;
    int y;
    for (y = 0; y < FIELD_HEIGHT; y++) {
        /* Shift cells left after the deleted column */
        int x;
        for (x = col; x < FIELD_WIDTH - 1; x++)
            f->cells[y][x] = f->cells[y][x + 1];
        f->cells[y][FIELD_WIDTH - 1] = CELL_EMPTY;
    }
}

/* Immunity — client-side state flag only; no field mutation needed */
/* Mutate pieces — client-side state flag only; no field mutation needed */

/* Darkness — obscure field: blank out all but bottom 2 rows client-side.
   Server side: just flag the effect; client render handles the mask. */
void field_darkness(Field *f) {
    /* Clear all visible rows except bottom 2 rows from client's view.
       We do this by storing a 'dark_rows' count in the field, but since
       Field has no such field, we encode it differently: zero out the top
       rows for the transmitted field so opponents see a blank board.
       The owning client renders its own field normally. */
    int y;
    for (y = 0; y < FIELD_HEIGHT - 2; y++)
        memset(f->cells[y], 0, FIELD_WIDTH);
}

/* Confusion — scramble left/right/rotate controls temporarily.
   Applied client-side as a timer flag; no field mutation. */

/* Encode field to ASCII string for network transmission.
   LineClash protocol: each cell is encoded as a character. */
void field_encode(Field *f, char *out, int maxlen) {
    int x, y, i = 0;
    /* Simple encoding: each cell 0-9 -> ASCII '0'-'9', then special offset */
    for (y = 0; y < FIELD_HEIGHT && i < maxlen - 1; y++) {
        for (x = 0; x < FIELD_WIDTH && i < maxlen - 1; x++) {
            uint8 v = f->cells[y][x];
            if (v == 0) out[i++] = '0';
            else if (v <= 9) out[i++] = '0' + v;
            else out[i++] = 'a' + (v - 10);
        }
    }
    out[i] = '\0';
}

void field_decode(Field *f, const char *data) {
    int x = 0, y = 0, i = 0;
    while (data[i] && y < FIELD_HEIGHT) {
        char c = data[i++];
        uint8 v;
        if (c >= '0' && c <= '9') v = (uint8)(c - '0');
        else if (c >= 'a' && c <= 'z') v = (uint8)(10 + c - 'a');
        else v = 0;
        f->cells[y][x] = v;
        x++;
        if (x >= FIELD_WIDTH) { x = 0; y++; }
    }
}

void field_add_special(Field *f, uint8 type) {
    if (f->special_count >= 64) return;
    f->specials[f->special_tail] = type;
    f->special_tail = (f->special_tail + 1) % 64;
    f->special_count++;
}

uint8 field_pop_special(Field *f) {
    uint8 v;
    if (f->special_count == 0) return 0;
    v = f->specials[f->special_head];
    f->special_head = (f->special_head + 1) % 64;
    f->special_count--;
    return v;
}
