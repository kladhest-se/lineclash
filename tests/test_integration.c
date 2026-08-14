/*
 * tests/test_integration.c — Integration tests for LineClash server/client protocol
 *
 * Starts lineclashd on a random port, connects raw TCP clients, and verifies
 * the server responds correctly to the full auth + lobby protocol.
 *
 * Build: cc -I../shared/include ../shared/src/net.c ../shared/src/field.c
 *            ../shared/src/piece.c ../shared/src/protocol.c test_integration.c -o test_integration -lm
 * Run:   ./test_integration [path/to/lineclashd]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include "lineclash.h"

/* ── test harness ─────────────────────────────────────────── */
static int g_pass = 0, g_fail = 0;
static pid_t g_server_pid = -1;
static int   g_port = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { printf("  PASS  %s\n", msg); g_pass++; } \
    else       { printf("  FAIL  %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while(0)

/* ── helpers ──────────────────────────────────────────────── */
static int pick_port(void) {
    /* Use a high random port to avoid conflicts */
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    return 34000 + (rand() % 1000);
}

static pid_t start_server(const char *binary, int port) {
    pid_t pid = fork();
    if (pid == 0) {
        /* child — redirect stdout/stderr to /dev/null to keep test output clean */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        execl(binary, binary, "--port", port_str, "--db", "/tmp/lineclash_test.db", NULL);
        exit(1);
    }
    return pid;
}

static void stop_server(void) {
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGTERM);
        int status;
        waitpid(g_server_pid, &status, 0);
        g_server_pid = -1;
        unlink("/tmp/lineclash_test.db");
    }
}

/* Wait for server to start listening */
static int wait_for_server(int port, int timeout_ms) {
    int waited = 0;
    while (waited < timeout_ms) {
        sock_t s = net_connect("127.0.0.1", port);
        if (s != INVALID_SOCK) { sock_close(s); return 1; }
        usleep(50000); /* 50ms */
        waited += 50;
    }
    return 0;
}

/* Read lines from socket until we see a line containing needle, timeout in ms */
static int recv_until(sock_t s, const char *needle, char *out, int outsz, int timeout_ms) {
    char buf[4096] = "";
    int blen = 0;
    char line[MAX_MSG_LEN];
    int waited = 0;

    while (waited < timeout_ms) {
        int n = net_recv(s, buf, &blen, line, sizeof(line));
        if (n < 0) return 0; /* disconnected */
        if (n == 1 && line[0]) {
            if (strstr(line, needle)) {
                if (out) snprintf(out, outsz, "%s", line);
                return 1;
            }
        }
        usleep(10000); /* 10ms */
        waited += 10;
    }
    return 0;
}

static sock_t connect_client(void) {
    sock_t s = net_connect("127.0.0.1", g_port);
    return s;
}

/* ── test: basic TCP connection ───────────────────────────── */
static void test_connect(void) {
    puts("server: TCP connect");
    sock_t s = connect_client();
    ASSERT(s != INVALID_SOCK, "server accepts TCP connection");
    if (s != INVALID_SOCK) sock_close(s);
}

/* ── test: server sends authchallenge on nick ─────────────── */
static void test_auth_challenge(void) {
    puts("server: auth handshake");
    sock_t s = connect_client();
    if (s == INVALID_SOCK) { printf("  SKIP  cannot connect\n"); return; }

    /* Send nick command to trigger auth */
    net_send(s, "nick testuser");

    char line[MAX_MSG_LEN];
    int got = recv_until(s, "authchallenge", line, sizeof(line), 2000);
    ASSERT(got, "server sends authchallenge after nick");
    if (got) {
        ASSERT(strstr(line, "authchallenge") != NULL, "response contains authchallenge");
    }
    sock_close(s);
}

/* ── test: register new account ──────────────────────────────*/
static void test_register(void) {
    puts("server: register new account");
    sock_t s = connect_client();
    if (s == INVALID_SOCK) { printf("  SKIP  cannot connect\n"); return; }

    net_send(s, "nick newuser_inttest");

    char line[MAX_MSG_LEN];
    /* Server should challenge with "new" for unknown nick */
    int got_challenge = recv_until(s, "authchallenge", line, sizeof(line), 2000);
    ASSERT(got_challenge, "server challenges new user");

    if (got_challenge && strstr(line, "new")) {
        /* Send a fake sha256 hash as password for new account */
        net_send(s, "authresp 0000000000000000000000000000000000000000000000000000000000000001");
        int got_ok = recv_until(s, "authok", line, sizeof(line), 2000);
        ASSERT(got_ok, "server accepts new account registration");
        if (got_ok) ASSERT(strstr(line, "new") != NULL, "authok confirms new account");
    }
    sock_close(s);
}

/* ── test: login existing account ────────────────────────────*/
static void test_login(void) {
    puts("server: login existing account");
    /* First register */
    sock_t s1 = connect_client();
    if (s1 == INVALID_SOCK) { printf("  SKIP  cannot connect\n"); return; }
    net_send(s1, "nick logintest");
    char line[MAX_MSG_LEN];
    if (recv_until(s1, "authchallenge new", line, sizeof(line), 2000)) {
        net_send(s1, "authresp aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        recv_until(s1, "authok", line, sizeof(line), 2000);
    }
    sock_close(s1);
    usleep(100000); /* give server time to persist */

    /* Now login with same nick — should get HMAC challenge (not "new") */
    sock_t s2 = connect_client();
    if (s2 == INVALID_SOCK) { printf("  SKIP  cannot connect\n"); return; }
    net_send(s2, "nick logintest");
    int got = recv_until(s2, "authchallenge", line, sizeof(line), 2000);
    ASSERT(got, "server challenges existing user");
    if (got) {
        ASSERT(strstr(line, "new") == NULL, "existing user gets HMAC challenge, not 'new'");
    }
    sock_close(s2);
}

/* ── test: server sends lobby list after auth ─────────────── */
static void test_lobby_list(void) {
    puts("server: lobby list after auth");
    sock_t s = connect_client();
    if (s == INVALID_SOCK) { printf("  SKIP  cannot connect\n"); return; }

    net_send(s, "nick lobbytest");
    char line[MAX_MSG_LEN];
    if (!recv_until(s, "authchallenge new", line, sizeof(line), 2000)) {
        printf("  SKIP  registration challenge not received\n");
        sock_close(s); return;
    }
    net_send(s, "authresp bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    int got_ok = recv_until(s, "authok", line, sizeof(line), 2000);
    ASSERT(got_ok, "authenticated successfully");

    if (got_ok) {
        int got_list = recv_until(s, "lobbylistend", line, sizeof(line), 2000);
        ASSERT(got_list, "server sends lobbylistend after auth");
    }
    sock_close(s);
}

/* ── test: join a room ───────────────────────────────────────*/
static void test_join_room(void) {
    puts("server: join room");
    sock_t s = connect_client();
    if (s == INVALID_SOCK) { printf("  SKIP  cannot connect\n"); return; }

    net_send(s, "nick jointest");
    char line[MAX_MSG_LEN];
    if (!recv_until(s, "authchallenge new", line, sizeof(line), 2000)) {
        printf("  SKIP  registration challenge not received\n");
        sock_close(s); return;
    }
    net_send(s, "authresp cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    if (!recv_until(s, "authok", line, sizeof(line), 2000)) {
        printf("  SKIP  auth failed\n"); sock_close(s); return;
    }
    recv_until(s, "lobbylistend", line, sizeof(line), 2000);

    /* Join room 1 */
    net_send(s, "lineclash_join 1");
    int got_num = recv_until(s, "playernum", line, sizeof(line), 2000);
    ASSERT(got_num, "server sends playernum after joining room");
    if (got_num) {
        int slot = 0;
        sscanf(line, "playernum %d", &slot);
        ASSERT(slot >= 1 && slot <= 8, "player slot is valid (1-8)");
    }
    sock_close(s);
}

/* ── test: two clients in same room see each other ───────────*/
static void test_two_clients(void) {
    puts("server: two clients in same room");

    /* Client A */
    sock_t sa = connect_client();
    if (sa == INVALID_SOCK) { printf("  SKIP  cannot connect A\n"); return; }
    net_send(sa, "nick player_a");
    char line[MAX_MSG_LEN];
    if (!recv_until(sa, "authchallenge new", line, sizeof(line), 2000)) {
        printf("  SKIP\n"); sock_close(sa); return;
    }
    net_send(sa, "authresp dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    recv_until(sa, "authok", line, sizeof(line), 2000);
    recv_until(sa, "lobbylistend", line, sizeof(line), 2000);
    net_send(sa, "lineclash_join 1");
    recv_until(sa, "playernum", line, sizeof(line), 2000);

    /* Client B */
    sock_t sb = connect_client();
    if (sb == INVALID_SOCK) { printf("  SKIP  cannot connect B\n"); return; }
    net_send(sb, "nick player_b");
    if (!recv_until(sb, "authchallenge new", line, sizeof(line), 2000)) {
        printf("  SKIP\n"); sock_close(sa); sock_close(sb); return;
    }
    net_send(sb, "authresp eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    recv_until(sb, "authok", line, sizeof(line), 2000);
    recv_until(sb, "lobbylistend", line, sizeof(line), 2000);
    net_send(sb, "lineclash_join 1");
    recv_until(sb, "playernum", line, sizeof(line), 2000);

    /* A should receive playerjoin for B */
    int a_sees_b = recv_until(sa, "player_b", line, sizeof(line), 2000);
    ASSERT(a_sees_b, "client A sees client B join the room");

    sock_close(sa);
    sock_close(sb);
}

/* ── test: server API responds ────────────────────────────── */
static void test_api(void) {
    puts("server: HTTP stats API");
    /* Connect to API port (server port + 1 by convention, or port 31458) */
    int api_port = g_port + 1;
    sock_t s = net_connect("127.0.0.1", api_port);
    if (s == INVALID_SOCK) {
        printf("  SKIP  API port not available (expected %d)\n", api_port);
        return;
    }
    net_send(s, "GET /api/status HTTP/1.0\r\nHost: localhost\r\n\r\n");
    char line[MAX_MSG_LEN];
    char buf[4096] = "";
    int blen = 0;
    int got_json = 0;
    for (int i = 0; i < 50; i++) {
        int n = net_recv(s, buf, &blen, line, sizeof(line));
        if (n < 0) break;
        if (n == 1 && strstr(line, "server_stats")) { got_json = 1; break; }
        usleep(20000);
    }
    ASSERT(got_json, "API returns JSON with server_stats");
    sock_close(s);
}

/* ── main ──────────────────────────────────────────────────── */
int main(int argc, char **argv) {
    const char *server_bin = argc > 1 ? argv[1] : "./server/lineclashd";

    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    puts("  LineClash integration tests");
    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    /* Check server binary exists */
    if (access(server_bin, X_OK) != 0) {
        printf("  ERROR: server binary not found or not executable: %s\n", server_bin);
        printf("         Build it first: make server\n");
        return 2;
    }

    net_init();
    g_port = pick_port();
    printf("  Server: %s\n  Port:   %d\n\n", server_bin, g_port);

    /* Start server */
    g_server_pid = start_server(server_bin, g_port);
    if (g_server_pid < 0) { puts("  ERROR: fork failed"); return 2; }

    atexit(stop_server);
    signal(SIGTERM, (void(*)(int))stop_server);

    if (!wait_for_server(g_port, 3000)) {
        puts("  ERROR: server did not start in 3s");
        stop_server();
        return 2;
    }
    puts("  Server started OK\n");

    test_connect();
    test_auth_challenge();
    test_register();
    test_login();
    test_lobby_list();
    test_join_room();
    test_two_clients();
    test_api();

    puts("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    puts("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    stop_server();
    return g_fail > 0 ? 1 : 0;
}
