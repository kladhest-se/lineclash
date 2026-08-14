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
        log_error("bind() failed: %d", sock_error());
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
int net_send(sock_t s, const char *msg) {
    char buf[MAX_MSG_LEN + 2];
    int len;
    snprintf(buf, sizeof(buf), "%s\n", msg);
    len = (int)strlen(buf);
#ifdef PLATFORM_WINDOWS
    return send(s, buf, len, 0) == len;
#else
    return send(s, buf, len, MSG_NOSIGNAL) == len;
#endif
}

/* ============================================================
 * Receive: appends data to buf, extracts one line if available.
 * Returns 1 if a line was extracted, 0 if more data needed, -1 on error/close.
 *
 * Non-blocking safe: EAGAIN/EWOULDBLOCK → return 0 (try again later).
 * recv() == 0 means the peer closed the connection gracefully → return -1.
 * ============================================================ */
int net_recv(sock_t s, char *buf, int *len, char *line, int maxline) {
    char tmp[RECV_BUF_SIZE];
    int n, i;
    int would_block = 0;
    if (s == INVALID_SOCK) return -1;

    /* Try to read more data */
#ifdef PLATFORM_WINDOWS
    n = recv(s, tmp, sizeof(tmp) - 1, 0);
    if (n == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) { n = 0; would_block = 1; }
        else return -1;
    } else if (n == 0) {
        would_block = -1; /* graceful close — drain buffer then signal -1 */
    }
#else
    n = recv(s, tmp, sizeof(tmp) - 1, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { n = 0; would_block = 1; }
        else { n = 0; would_block = -1; } /* real error — drain buffer then -1 */
    } else if (n == 0) {
        would_block = -1; /* graceful close — drain buffer then signal -1 */
    }
#endif

    /* Append received data to buffer */
    if (n > 0) {
        int space = RECV_BUF_SIZE - 1 - *len;
        if (space <= 0) {
            /* Buffer full: try to make room by discarding up to first newline */
            char *nl = (char *)memchr(buf, '\n', *len);
            if (!nl) nl = (char *)memchr(buf, '\r', *len);
            if (nl) {
                int skip = (int)(nl - buf) + 1;
                *len -= skip;
                if (*len > 0) memmove(buf, buf + skip, *len);
                buf[*len] = '\0';
                space = RECV_BUF_SIZE - 1 - *len;
            }
        }
        if (n > space) n = space; /* clamp to available space */
        if (n > 0) {
            memcpy(buf + *len, tmp, n);
            *len += n;
            buf[*len] = '\0';
        }
    }

    /* Look for a complete line in the buffer */
    for (i = 0; i < *len; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            int linelen = i;
            if (linelen >= maxline) linelen = maxline - 1;
            memcpy(line, buf, linelen);
            line[linelen] = '\0';
            /* Consume line (and any trailing \r\n) from buffer */
            i++;
            while (i < *len && (buf[i] == '\n' || buf[i] == '\r')) i++;
            *len -= i;
            if (*len > 0) memmove(buf, buf + i, *len);
            buf[*len] = '\0';
            return 1;
        }
    }

    /* No complete line yet */
    return (would_block == -1) ? -1 : 0; /* -1 signals closed connection */
}
