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
static char test_dir[] = "/tmp/lineclash-tests-XXXXXX";
static char config_path[512];

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
        /* Keep startup diagnostics visible. */
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        execl(binary, binary, "--port", port_str, "--config", config_path, NULL);
        perror("exec server");
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
        const char *files[] = {"server.cfg", "database.sqlite3", "database.sqlite3-wal",
                               "database.sqlite3-shm", "server.log"};
        for (unsigned i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
            char path[512]; snprintf(path, sizeof(path), "%s/%s", test_dir, files[i]); unlink(path);
        }
        rmdir(test_dir);
    }
}

/* Wait for server to start listening */
static int wait_for_server(int port, int timeout_ms) {
    int waited = 0;
    while (waited < timeout_ms) {
        int status;
        if (waitpid(g_server_pid, &status, WNOHANG) == g_server_pid) {
            fprintf(stderr, "Server exited early (status %d)\n", status);
            g_server_pid = -1; return 0;
        }
        sock_t s = net_connect("127.0.0.1", port);
        if (s != INVALID_SOCK) { sock_close(s); return 1; }
        usleep(50000); /* 50ms */
        waited += 50;
    }
    return 0;
}

/* Read lines from socket until we see a line containing needle, timeout in ms */
static int recv_until(sock_t s, const char *needle, char *out, int outsz, int timeout_ms) {
    char line[MAX_MSG_LEN]; int len = 0, waited = 0;
    /* Consume one byte at a time: do not lose later lines between calls. */
    while (waited < timeout_ms) {
        char ch; net_flush_all();
        int n = (int)recv(s, &ch, 1, 0);
        if (n == 0) return 0;
        if (n < 0) { usleep(10000); waited += 10; continue; }
        if (ch == '\n' || ch == '\r') {
            line[len] = 0;
            if (strstr(line, needle)) {
                if (out) snprintf(out, outsz, "%s", line);
                return 1;
            }
            len = 0;
        } else if (len < (int)sizeof(line)-1) line[len++] = ch;
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
    if (s == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect\n"); return; }

    /* Send nick command to trigger auth */
    net_send(s, "authnick testuser");

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
    if (s == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect\n"); return; }

    net_send(s, "authnick newuser_inttest");

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
    if (s1 == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect\n"); return; }
    net_send(s1, "authnick logintest");
    char line[MAX_MSG_LEN];
    if (recv_until(s1, "authchallenge new", line, sizeof(line), 2000)) {
        net_send(s1, "authresp aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        recv_until(s1, "authok", line, sizeof(line), 2000);
    }
    sock_close(s1);
    usleep(100000); /* give server time to persist */

    /* Now login with same nick — should get HMAC challenge (not "new") */
    sock_t s2 = connect_client();
    if (s2 == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect\n"); return; }
    net_send(s2, "authnick logintest");
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
    if (s == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect\n"); return; }

    net_send(s, "authnick lobbytest");
    char line[MAX_MSG_LEN];
    if (!recv_until(s, "authchallenge new", line, sizeof(line), 2000)) {
        g_fail++; printf("  FAIL  registration challenge not received\n");
        sock_close(s); return;
    }
    net_send(s, "authresp bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    int got_ok = recv_until(s, "authok", line, sizeof(line), 2000);
    ASSERT(got_ok, "authenticated successfully");

    if (got_ok) {
        net_send(s, "listlobbies");
        int got_list = recv_until(s, "lobbylistend", line, sizeof(line), 2000);
        ASSERT(got_list, "server sends lobbylistend after auth");
    }
    sock_close(s);
}

/* ── test: join a room ───────────────────────────────────────*/
static void test_join_room(void) {
    puts("server: join room");
    sock_t s = connect_client();
    if (s == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect\n"); return; }

    net_send(s, "authnick jointest");
    char line[MAX_MSG_LEN];
    if (!recv_until(s, "authchallenge new", line, sizeof(line), 2000)) {
        g_fail++; printf("  FAIL  registration challenge not received\n");
        sock_close(s); return;
    }
    net_send(s, "authresp cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    if (!recv_until(s, "authok", line, sizeof(line), 2000)) {
        g_fail++; printf("  FAIL  auth failed\n"); sock_close(s); return;
    }
    net_send(s, "listlobbies");
    recv_until(s, "lobbylistend", line, sizeof(line), 2000);

    /* Join room 1 */
    net_send(s, "lineclash_join jointest - 1");
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
    if (sa == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect A\n"); return; }
    net_send(sa, "authnick player_a");
    char line[MAX_MSG_LEN];
    if (!recv_until(sa, "authchallenge new", line, sizeof(line), 2000)) {
        g_fail++; printf("  FAIL\n"); sock_close(sa); return;
    }
    net_send(sa, "authresp dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    recv_until(sa, "authok", line, sizeof(line), 2000);
    net_send(sa, "listlobbies");
    recv_until(sa, "lobbylistend", line, sizeof(line), 2000);
    net_send(sa, "lineclash_join player_a - 1");
    recv_until(sa, "playernum", line, sizeof(line), 2000);

    /* Client B */
    sock_t sb = connect_client();
    if (sb == INVALID_SOCK) { g_fail++; printf("  FAIL  cannot connect B\n"); return; }
    net_send(sb, "authnick player_b");
    if (!recv_until(sb, "authchallenge new", line, sizeof(line), 2000)) {
        g_fail++; printf("  FAIL\n"); sock_close(sa); sock_close(sb); return;
    }
    net_send(sb, "authresp eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    recv_until(sb, "authok", line, sizeof(line), 2000);
    net_send(sb, "listlobbies");
    recv_until(sb, "lobbylistend", line, sizeof(line), 2000);
    net_send(sb, "lineclash_join player_b - 1");
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
    sock_t s = net_connect("127.0.0.1", g_port + 1);
    ASSERT(s != INVALID_SOCK, "API port is available");
    if (s == INVALID_SOCK) return;
    const char *request = "GET / HTTP/1.0\r\n\r\n";
    send(s, request, (int)strlen(request), 0);
    char response[65536]; int len = 0;
    for (int i = 0; i < 200; i++) {
        int n = (int)recv(s, response + len, sizeof(response)-1-len, 0);
        if (n > 0) len += n;
        else if (n == 0) break;
        if (len == sizeof(response)-1) break;
        usleep(10000);
    }
    response[len] = 0;
    ASSERT(strstr(response, "server_stats") != NULL, "API returns server_stats");
    ASSERT(len >= 2 && response[len-1] == '}' && response[len-2] == '}',
           "API closes server_stats and root JSON objects");
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

    if (!mkdtemp(test_dir)) { perror("mkdtemp"); return 2; }
    snprintf(config_path, sizeof(config_path), "%s/server.cfg", test_dir);
    FILE *cfg = fopen(config_path, "w");
    if (!cfg) { perror("config"); return 2; }
    fprintf(cfg, "db_file = %s/database.sqlite3\nlog_file = %s/server.log\n"
                 "status_port = %d\nnum_lobbies = 3\n", test_dir, test_dir, g_port+1);
    fclose(cfg);
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
