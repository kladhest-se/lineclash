/*
 * tests/test_field.c — Unit tests for field + piece shared logic
 * Build: cc -I../shared/include ../shared/src/field.c ../shared/src/piece.c test_field.c -o test_field
 * Run:   ./test_field
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lineclash.h"

static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { printf("  PASS  %s\n", msg); g_pass++; } \
    else       { printf("  FAIL  %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while(0)

/* ── field_init ──────────────────────────────────────────── */
static void test_field_init(void) {
    puts("field_init");
    Field f;
    field_init(&f);
    ASSERT(f.lines  == 0, "lines starts at 0");
    ASSERT(f.level  == 1, "level starts at 1");
    ASSERT(f.score  == 0, "score starts at 0");
    /* All cells empty */
    int empty = 1;
    for (int y = 0; y < FIELD_HEIGHT; y++)
        for (int x = 0; x < FIELD_WIDTH; x++)
            if (f.cells[y][x]) { empty = 0; break; }
    ASSERT(empty, "all cells empty after init");
}

/* ── field_add_garbage / field_clear_lines ───────────────── */
static void test_garbage(void) {
    puts("field_add_garbage + field_clear_lines");
    Field f;
    field_init(&f);

    field_add_garbage(&f, 1);
    /* Bottom row should have at least one filled cell */
    int filled = 0;
    for (int x = 0; x < FIELD_WIDTH; x++)
        if (f.cells[FIELD_HEIGHT-1][x]) filled++;
    ASSERT(filled > 0, "garbage adds cells to bottom row");

    /* Fill bottom row completely so it clears */
    for (int x = 0; x < FIELD_WIDTH; x++)
        f.cells[FIELD_HEIGHT-1][x] = 1;
    int cleared = field_clear_lines(&f);
    ASSERT(cleared == 1, "full row clears (returns 1)");
    /* Bottom row should be empty again */
    int empty = 1;
    for (int x = 0; x < FIELD_WIDTH; x++)
        if (f.cells[FIELD_HEIGHT-1][x]) { empty = 0; break; }
    ASSERT(empty, "bottom row empty after clear");
}

static void test_garbage_multi(void) {
    puts("field_add_garbage multi-line");
    Field f;
    field_init(&f);
    field_add_garbage(&f, 4);
    /* Bottom 4 rows should be non-empty */
    int non_empty_rows = 0;
    for (int y = FIELD_HEIGHT-4; y < FIELD_HEIGHT; y++) {
        for (int x = 0; x < FIELD_WIDTH; x++)
            if (f.cells[y][x]) { non_empty_rows++; break; }
    }
    ASSERT(non_empty_rows == 4, "4 garbage lines fill 4 rows");

    /* Fill all 4 rows completely */
    for (int y = FIELD_HEIGHT-4; y < FIELD_HEIGHT; y++)
        for (int x = 0; x < FIELD_WIDTH; x++)
            f.cells[y][x] = 1;
    int cleared = field_clear_lines(&f);
    ASSERT(cleared == 4, "4 full rows clear (returns 4)");
}

/* ── piece_init + piece_spawn ────────────────────────────── */
static void test_piece(void) {
    puts("piece_init + piece_spawn");
    Field f;
    field_init(&f);
    Piece p;
    for (int t = 0; t < NUM_PIECES; t++) {
        piece_init(&p, t);
        ASSERT(p.type == t, "piece type set correctly");
        ASSERT(p.rotation == 0, "piece rotation starts at 0 (rotation field)");
        int ok = piece_spawn(&f, &p);
        ASSERT(ok,         "piece spawns on empty field");
    }
}

/* ── piece_move_left / piece_move_right ──────────────────── */
static void test_piece_move(void) {
    puts("piece_move_left + piece_move_right");
    Field f;
    field_init(&f);
    Piece p;
    piece_init(&p, 0); /* I piece */
    piece_spawn(&f, &p);
    int start_x = p.x;
    int moved_r = piece_move_right(&f, &p);
    ASSERT(moved_r,        "can move right on empty field");
    ASSERT(p.x == start_x + 1, "x increased by 1 after move right");
    int moved_l = piece_move_left(&f, &p);
    ASSERT(moved_l,        "can move left on empty field");
    ASSERT(p.x == start_x, "x back to start after move left");
}

/* ── piece_move_down ─────────────────────────────────────── */
static void test_piece_drop(void) {
    puts("piece_move_down");
    Field f;
    field_init(&f);
    Piece p;
    piece_init(&p, 4); /* S piece */
    piece_spawn(&f, &p);
    int moved = 1, steps = 0;
    while (moved) {
        moved = piece_move_down(&f, &p);
        steps++;
    }
    ASSERT(steps > 1, "piece falls more than 1 step before landing");
}

/* ── field boundaries ────────────────────────────────────── */
static void test_field_size(void) {
    puts("field constants");
    ASSERT(FIELD_WIDTH  == 12, "FIELD_WIDTH is 12");
    ASSERT(FIELD_HEIGHT == 22, "FIELD_HEIGHT is 22");
    ASSERT(NUM_PIECES   ==  7, "NUM_PIECES is 7");
}

/* ── field_clear_specials ────────────────────────────────── */
static void test_clear_specials(void) {
    puts("field_clear_specials");
    Field f;
    field_init(&f);
    /* Place some specials (values > 8) */
    f.cells[FIELD_HEIGHT-1][0] = 9;  /* special block */
    f.cells[FIELD_HEIGHT-1][1] = 10;
    f.cells[FIELD_HEIGHT-1][2] = 1;  /* normal block — should stay */
    field_clear_specials(&f);
    ASSERT(f.cells[FIELD_HEIGHT-1][0] == CELL_GARBAGE, "special block replaced with garbage");
    ASSERT(f.cells[FIELD_HEIGHT-1][1] == CELL_GARBAGE, "special block replaced with garbage");
    ASSERT(f.cells[FIELD_HEIGHT-1][2] == 1,            "normal block unchanged");
}

/* ── main ────────────────────────────────────────────────── */
int main(void) {
    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    puts("  LineClash unit tests");
    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    test_field_size();
    test_field_init();
    test_garbage();
    test_garbage_multi();
    test_piece();
    test_piece_move();
    test_piece_drop();
    test_clear_specials();

    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    return g_fail > 0 ? 1 : 0;
}
