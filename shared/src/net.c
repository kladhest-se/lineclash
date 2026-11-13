/* net.c - Cross-platform networking */
#include "../../shared/include/lineclash.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Logging — tees to stdout and an optional log file
 * ============================================================ */
static FILE *g_net_logfile = NULL;
static int   g_net_quiet   = 0;   /* 1 = suppress log_info stdout output */

void log_set_file(FILE *f) { g_net_logfile = f; }
void log_set_quiet(int q)  { g_net_quiet = q; }

void (*g_log_stat_hook)(void) = NULL; /* set by server to count log lines */

void log_info(const char *fmt, ...) {
    va_list ap;
    if (g_log_stat_hook) g_log_stat_hook();
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    if (!g_net_quiet) {
        printf("[%s INFO] ", ts);
        va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
        printf("\n"); fflush(stdout);
    }
    if (g_net_logfile) {
        fprintf(g_net_logfile,"[%s INFO] ",ts);
        va_start(ap, fmt); vfprintf(g_net_logfile,fmt,ap); va_end(ap);
        fprintf(g_net_logfile,"\n"); fflush(g_net_logfile);
    }
}

void log_error(const char *fmt, ...) {
    va_list ap;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(stderr,"[%s ERROR] ",ts);
    va_start(ap, fmt); vfprintf(stderr,fmt,ap); va_end(ap);
    fprintf(stderr,"\n"); fflush(stderr);
    if (g_net_logfile) {
        fprintf(g_net_logfile,"[%s ERROR] ",ts);
        va_start(ap, fmt); vfprintf(g_net_logfile,fmt,ap); va_end(ap);
        fprintf(g_net_logfile,"\n"); fflush(g_net_logfile);
    }
}

/* ============================================================
 * Utility: string split
 * ============================================================ */
int str_split(const char *s, char delim, char parts[][MAX_MSG_LEN], int max) {
    int count = 0;
    const char *p = s;
    const char *start = s;
    int len;

    while (count < max) {
        if (*p == delim || *p == '\0') {
            len = (int)(p - start);
            if (len >= MAX_MSG_LEN) len = MAX_MSG_LEN - 1;
            memcpy(parts[count], start, len);
            parts[count][len] = '\0';
            count++;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    return count;
}

/* ============================================================
 * Network init/cleanup
 * ============================================================ */
int net_init(void) {
#ifdef PLATFORM_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        log_error("WSAStartup failed: %d", WSAGetLastError());
        return 0;
    }
#endif
    return 1;
}

void net_cleanup(void) {
#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

/* ============================================================
 * Set socket non-blocking
 * ============================================================ */
void net_set_nonblocking(sock_t s) {
#ifdef PLATFORM_WINDOWS
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0) fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

/* ============================================================
 * Listen socket
 * ============================================================ */
sock_t net_listen(int port) {
    sock_t s;
    struct sockaddr_in addr;
    int opt = 1;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) {
        log_error("socket() failed: %d", sock_error());
        return INVALID_SOCK;
    }

#ifdef PLATFORM_WINDOWS
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16)port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        int error=sock_error();
#if defined(PLATFORM_WINDOWS)
        int occupied=(error==WSAEADDRINUSE);
#else
        int occupied=(error==EADDRINUSE);
#endif
        if(occupied)log_error("TCP port %d is already in use. Stop the existing listener or select another port (--port PORT for the game server).",port);
        else log_error("bind() failed on TCP port %d: %d",port,error);
        sock_close(s);
        return INVALID_SOCK;
    }

    if (listen(s, 10) < 0) {
        log_error("listen() failed: %d", sock_error());
        sock_close(s);
        return INVALID_SOCK;
    }

    net_set_nonblocking(s);
    log_info("Listening on port %d", port);
    return s;
}

/* ============================================================
 * Connect to server
 * ============================================================ */
sock_t net_connect(const char *host, int port) {
    struct sockaddr_in addr;
    struct hostent *he;
    sock_t s;
    int rc;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) {
        log_error("socket() failed");
        return INVALID_SOCK;
    }

    /* DNS lookup — gethostbyname is blocking but typically fast for NXDOMAIN.
     * For a completely unroutable host this may take up to the OS resolver
     * timeout (~5s). We accept this; it only affects the initial probe. */
    he = gethostbyname(host);
    if (!he) {
        log_error("gethostbyname(%s) failed", host);
        sock_close(s);
        return INVALID_SOCK;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16)port);
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

    /* Set non-blocking so connect() returns immediately, then wait with
     * select() for up to NET_CONNECT_TIMEOUT_MS ms. This prevents hanging
     * the render loop when a host is unreachable or the port is filtered. */
    net_set_nonblocking(s);
    rc = connect(s, (struct sockaddr*)&addr, sizeof(addr));

#if defined(PLATFORM_WINDOWS)
    if (rc < 0 && sock_error() != WSAEWOULDBLOCK) {
#else
    if (rc < 0 && errno != EINPROGRESS) {
#endif
        log_error("connect() failed immediately: %d", sock_error());
        sock_close(s);
        return INVALID_SOCK;
    }

    if (rc != 0) {
        /* Connection in progress — wait with select() */
        fd_set wfds, efds;
        struct timeval tv;
        FD_ZERO(&wfds); FD_SET(s, &wfds);
        FD_ZERO(&efds); FD_SET(s, &efds);
        tv.tv_sec  = NET_CONNECT_TIMEOUT_MS / 1000;
        tv.tv_usec = (NET_CONNECT_TIMEOUT_MS % 1000) * 1000;
        rc = select((int)s + 1, NULL, &wfds, &efds, &tv);
        if (rc <= 0 || FD_ISSET(s, &efds)) {
            /* Timed out or got an error on the socket */
            if (rc == 0) log_error("connect() timed out (%dms) to %s:%d",
                                    NET_CONNECT_TIMEOUT_MS, host, port);
            else          log_error("connect() failed (select error) to %s:%d", host, port);
            sock_close(s);
            return INVALID_SOCK;
        }
        /* Verify connection actually succeeded (getsockopt SO_ERROR) */
        {int err = 0; socklen_t el = sizeof(err);
         getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
         if (err) {
             log_error("connect() error %d to %s:%d", err, host, port);
             sock_close(s);
             return INVALID_SOCK;
         }}
    }

    log_info("Connected to %s:%d", host, port);
    return s;
}


/* ============================================================
 * Non-blocking connect start — returns socket immediately.
 * The socket will be writable (via select) when connected.
 * Use net_connect_nb_poll() each frame to check progress.
 * Returns INVALID_SOCK if DNS fails or socket can't be created.
 * ============================================================ */
sock_t net_connect_nb(const char *host, int port) {
    struct sockaddr_in addr;
    struct hostent *he;
    sock_t s;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) return INVALID_SOCK;

    /* DNS — blocking, but instant for IP addresses and typically fast
     * for hostnames on local networks (<100ms). */
    he = gethostbyname(host);
    if (!he) { sock_close(s); return INVALID_SOCK; }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16)port);
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

    net_set_nonblocking(s);
    connect(s, (struct sockaddr*)&addr, sizeof(addr));
    /* Ignore return value — EINPROGRESS/WSAEWOULDBLOCK is expected */
    return s;
}

/* ============================================================
 * Poll a non-blocking connect in progress.
 * Returns:  1 = connected successfully
 *           0 = still in progress (call again next frame)
 *          -1 = failed / timed out
 * ============================================================ */
int net_connect_nb_poll(sock_t s) {
    fd_set wfds, efds;
    struct timeval tv = {0, 0};  /* zero timeout — non-blocking poll */
    int err = 0;
    socklen_t el = sizeof(err);

    if (s == INVALID_SOCK) return -1;

    FD_ZERO(&wfds); FD_SET(s, &wfds);
    FD_ZERO(&efds); FD_SET(s, &efds);
    if (select((int)s + 1, NULL, &wfds, &efds, &tv) < 0) return -1;
    if (FD_ISSET(s, &efds)) return -1;  /* error */
    if (!FD_ISSET(s, &wfds)) return 0;  /* still waiting */
    /* Writable — verify with SO_ERROR */
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
    return (err == 0) ? 1 : -1;
}

/* ============================================================
 * Send a complete line (appends \n)
 * ============================================================ */
/* Bounded queues preserve whole lines across partial writes/EAGAIN. Each
 * application pumps these once per frame, including sockets with no input. */
#define NET_OUTPUT_LIMIT (64 * 1024)
#define NET_OUTPUT_TIMEOUT 5
struct OutputQueue {
    sock_t sock;
    size_t len;
    time_t stalled_since;
    struct OutputQueue *next;
    char data[NET_OUTPUT_LIMIT];
};
static struct OutputQueue *outputs;

static int send_error_retry(void) {
#ifdef PLATFORM_WINDOWS
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

static void output_discard(sock_t s) {
    struct OutputQueue **q = &outputs;
    while (*q) {
        if ((*q)->sock == s) {
            struct OutputQueue *old = *q;
            *q = old->next; free(old); return;
        }
        q = &(*q)->next;
    }
}

void net_close(sock_t s) {
    output_discard(s);
#ifdef PLATFORM_WINDOWS
    closesocket(s);
#else
    close(s);
#endif
}

/* On permanent failure, shutdown makes the owner's normal receive path
 * disconnect the peer; never close a descriptor behind the owner's back. */
static int output_flush(struct OutputQueue *q) {
    while (q->len) {
#ifdef PLATFORM_WINDOWS
        int n = send(q->sock, q->data, (int)q->len, 0);
#else
        int n = (int)send(q->sock, q->data, q->len, MSG_NOSIGNAL);
#endif
        if (n > 0) {
            q->len -= n;
            memmove(q->data, q->data + n, q->len);
            q->stalled_since = time(NULL);
        } else {
            if (n < 0 && send_error_retry() &&
                time(NULL) - q->stalled_since < NET_OUTPUT_TIMEOUT) return 1;
            shutdown(q->sock, 2); return 0;
        }
    }
    return 1;
}

void net_flush_all(void) {
    struct OutputQueue **at = &outputs;
    while (*at) {
        struct OutputQueue *q = *at;
        if (!output_flush(q) || !q->len) {
            *at = q->next; free(q);
        } else at = &q->next;
    }
}

int net_send(sock_t s, const char *msg) {
    struct OutputQueue *q;
    size_t len = strlen(msg);
    if (s == INVALID_SOCK || len > MAX_MSG_LEN) return 0;
    for (q = outputs; q && q->sock != s; q = q->next) {}
    if (!q) {
        q = (struct OutputQueue *)calloc(1, sizeof(*q));
        if (!q) { shutdown(s, 2); return 0; }
        q->sock = s; q->stalled_since = time(NULL);
        q->next = outputs; outputs = q;
    }
    if (q->len + len + 1 > sizeof(q->data)) {
        shutdown(s, 2); output_discard(s); return 0;
    }
    memcpy(q->data + q->len, msg, len);
    q->len += len; q->data[q->len++] = '\n';
    if (!output_flush(q)) { output_discard(s); return 0; }
    if (!q->len) output_discard(s);
    return 1; /* accepted for ordered delivery */
}

/* ============================================================
 * Receive: appends data to buf, extracts one line if available.
 * Returns 1 if a line was extracted, 0 if more data needed, -1 on error/close.
 *
 * Non-blocking safe: EAGAIN/EWOULDBLOCK → return 0 (try again later).
 * recv() == 0 means the peer closed the connection gracefully → return -1.
 * ============================================================ */
/* Extract before reading so coalesced lines never get discarded. Oversized
 * or NUL-containing protocol input is rejected, not silently truncated. */
static int extract_line(char *buf, int *len, char *line, int maxline) {
    int i;
    for (i = 0; i < *len; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            int used = i + 1;
            if (i >= maxline) return -1;
            memcpy(line, buf, i); line[i] = 0;
            while (used < *len && (buf[used] == '\n' || buf[used] == '\r')) used++;
            *len -= used; memmove(buf, buf + used, *len); buf[*len] = 0;
            return 1;
        }
    }
    return *len >= maxline ? -1 : 0;
}

int net_recv(sock_t s, char *buf, int *len, char *line, int maxline) {
    int r, n, space;
    if (s == INVALID_SOCK || maxline < 1 || *len < 0 || *len >= RECV_BUF_SIZE) return -1;
    net_flush_all();
    r = extract_line(buf, len, line, maxline);
    if (r) return r;
    space = RECV_BUF_SIZE - 1 - *len;
    if (!space) return -1;
    n = (int)recv(s, buf + *len, space, 0);
    if (n == 0) return -1;
    if (n < 0) return send_error_retry() ? 0 : -1;
    if (memchr(buf + *len, 0, n)) return -1;
    *len += n; buf[*len] = 0;
    return extract_line(buf, len, line, maxline);
}

/* Reconnect attempts never sleep or wait for connect inside the render loop. */
uint64_t net_now_ms(void) {
#ifdef PLATFORM_WINDOWS
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}
void net_peer_address(sock_t s, char *out, size_t size, const char *fallback) {
    struct sockaddr_in addr; socklen_t len = sizeof(addr);
    if (getpeername(s, (struct sockaddr *)&addr, &len) == 0)
        snprintf(out, size, "%s", inet_ntoa(addr.sin_addr));
    else snprintf(out, size, "%s", fallback);
}
void net_reconnect_stop(NetReconnect *r) {
    if (r->active && r->candidate != INVALID_SOCK) sock_close(r->candidate);
    r->candidate = INVALID_SOCK; r->active = 0;
}
void net_reconnect_begin(NetReconnect *r, uint64_t now, int seconds) {
    memset(r, 0, sizeof(*r)); r->candidate = INVALID_SOCK;
    r->active = seconds > 0;
    r->deadline = now + (uint64_t)(seconds > 0 ? seconds : 0) * 1000;
    r->due = now + 250; /* allow the server to observe the old connection close */
}
sock_t net_reconnect_poll(NetReconnect *r, const char *host, int port, uint64_t now) {
    if (!r->active) return INVALID_SOCK;
    if (now >= r->deadline) { net_reconnect_stop(r); return INVALID_SOCK; }
    if (r->candidate == INVALID_SOCK) {
        if (now < r->due) return INVALID_SOCK;
        r->attempts++;
        r->candidate = net_connect_nb(host, port);
        r->attempt_deadline = now + 1500;
        r->due = now + 1000;
        if (r->candidate == INVALID_SOCK) return INVALID_SOCK;
    }
    {
        int ready = net_connect_nb_poll(r->candidate);
        if (ready == 1) {
            sock_t s = r->candidate; r->candidate = INVALID_SOCK; return s;
        }
        if (ready < 0 || now >= r->attempt_deadline) {
            sock_close(r->candidate); r->candidate = INVALID_SOCK;
        }
    }
    return INVALID_SOCK;
}
