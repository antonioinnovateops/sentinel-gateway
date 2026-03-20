/**
 * @file tcp_transport.c
 * @brief TCP transport using epoll for Linux gateway
 * @implements SWE-030 through SWE-034
 */

#define _POSIX_C_SOURCE 200809L

#include "tcp_transport.h"
#include "../common/wire_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define MAX_EVENTS     8
#define RX_BUF_SIZE    2048

/* Per-connection receive buffer state */
typedef struct {
    uint8_t buf[RX_BUF_SIZE];
    size_t pos;
} conn_rx_state_t;

/* Socket state */
static int g_epoll_fd = -1;
static int g_cmd_fd = -1;       /* Connected to MCU port 5000 */
static int g_tel_listen_fd = -1; /* Listening on port 5001 */
static int g_tel_fd = -1;       /* Accepted telemetry client (MCU) */
static int g_diag_listen_fd = -1; /* Listening on port 5002 */
static int g_diag_fd = -1;      /* Accepted diagnostics client */

static transport_recv_cb_t g_recv_cb = NULL;
static void *g_recv_ctx = NULL;
static transport_diag_cb_t g_diag_cb = NULL;
static void *g_diag_ctx = NULL;

/* Per-connection receive buffers for wire frame reassembly */
static conn_rx_state_t g_cmd_rx;   /* Command channel (port 5000) */
static conn_rx_state_t g_tel_rx;   /* Telemetry channel (port 5001) */

/* Diagnostic receive buffer (text, line-based) */
static uint8_t g_diag_rx_buf[512];
static size_t g_diag_rx_pos = 0U;

/* Forward declarations */
static void process_diag_data(int fd);

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { return -1; }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int create_listen_socket(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }

    int opt = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 2) < 0) {
        close(fd);
        return -1;
    }

    (void)set_nonblocking(fd);
    return fd;
}

static void epoll_add(int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    (void)epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

sentinel_err_t transport_init(void)
{
    g_epoll_fd = epoll_create1(0);
    if (g_epoll_fd < 0) {
        return SENTINEL_ERR_INTERNAL;
    }
    /* Initialize per-connection receive buffers */
    (void)memset(&g_cmd_rx, 0, sizeof(g_cmd_rx));
    (void)memset(&g_tel_rx, 0, sizeof(g_tel_rx));
    return SENTINEL_OK;
}

sentinel_err_t transport_connect_command(const char *mcu_ip)
{
    g_cmd_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_cmd_fd < 0) {
        return SENTINEL_ERR_COMM;
    }

    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SENTINEL_PORT_COMMAND);
    if (inet_pton(AF_INET, mcu_ip, &addr.sin_addr) <= 0) {
        close(g_cmd_fd);
        g_cmd_fd = -1;
        return SENTINEL_ERR_INVALID_ARG;
    }

    if (connect(g_cmd_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(g_cmd_fd);
        g_cmd_fd = -1;
        return SENTINEL_ERR_COMM;
    }

    int opt = 1;
    (void)setsockopt(g_cmd_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    (void)set_nonblocking(g_cmd_fd);
    epoll_add(g_cmd_fd);

    return SENTINEL_OK;
}

sentinel_err_t transport_listen_telemetry(void)
{
    g_tel_listen_fd = create_listen_socket(SENTINEL_PORT_TELEMETRY);
    if (g_tel_listen_fd < 0) {
        return SENTINEL_ERR_COMM;
    }
    epoll_add(g_tel_listen_fd);
    return SENTINEL_OK;
}

sentinel_err_t transport_listen_diagnostics(void)
{
    g_diag_listen_fd = create_listen_socket(SENTINEL_PORT_DIAG);
    if (g_diag_listen_fd < 0) {
        return SENTINEL_ERR_COMM;
    }
    epoll_add(g_diag_listen_fd);
    fprintf(stderr, "[TRANSPORT] diag listen fd=%d, epoll_fd=%d\n",
            g_diag_listen_fd, g_epoll_fd);
    return SENTINEL_OK;
}

static void process_received_data(int fd)
{
    /* Select the appropriate per-connection buffer */
    conn_rx_state_t *rx = NULL;
    if (fd == g_cmd_fd) {
        rx = &g_cmd_rx;
    } else if (fd == g_tel_fd) {
        rx = &g_tel_rx;
    } else {
        return; /* Unknown fd — ignore */
    }

    ssize_t n = read(fd, rx->buf + rx->pos, RX_BUF_SIZE - rx->pos);
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            /* Peer disconnected — clean up */
            (void)epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            if (fd == g_tel_fd) { g_tel_fd = -1; }
            if (fd == g_cmd_fd) { g_cmd_fd = -1; }
            rx->pos = 0U;
        }
        return;
    }
    rx->pos += (size_t)n;

    /* Try to extract complete wire frames */
    while (rx->pos >= WIRE_FRAME_HEADER_SIZE) {
        uint8_t msg_type;
        uint32_t payload_len;
        sentinel_err_t err = wire_frame_decode_header(rx->buf, rx->pos,
                                                       &msg_type, &payload_len);
        if (err != SENTINEL_OK) {
            rx->pos = 0U; /* Discard corrupt data */
            break;
        }

        size_t frame_len = WIRE_FRAME_HEADER_SIZE + payload_len;
        if (rx->pos < frame_len) {
            break; /* Incomplete frame, wait for more data */
        }

        /* Dispatch complete frame */
        if (g_recv_cb != NULL) {
            g_recv_cb(msg_type,
                      rx->buf + WIRE_FRAME_HEADER_SIZE,
                      payload_len,
                      g_recv_ctx);
        }

        /* Shift remaining data */
        if (rx->pos > frame_len) {
            (void)memmove(rx->buf, rx->buf + frame_len,
                          rx->pos - frame_len);
        }
        rx->pos -= frame_len;
    }
}

sentinel_err_t transport_poll(int timeout_ms)
{
    struct epoll_event events[MAX_EVENTS];
    int nfds = epoll_wait(g_epoll_fd, events, MAX_EVENTS, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) { return SENTINEL_OK; }
        return SENTINEL_ERR_INTERNAL;
    }

    for (int i = 0; i < nfds; i++) {
        int fd = events[i].data.fd;

        /* Accept new telemetry connection */
        if (fd == g_tel_listen_fd) {
            int new_fd = accept(g_tel_listen_fd, NULL, NULL);
            if (new_fd >= 0) {
                if (g_tel_fd >= 0) { close(g_tel_fd); }
                g_tel_fd = new_fd;
                (void)set_nonblocking(g_tel_fd);
                epoll_add(g_tel_fd);
            }
            continue;
        }

        /* Accept new diagnostics connection */
        if (fd == g_diag_listen_fd) {
            int new_fd = accept(g_diag_listen_fd, NULL, NULL);
            if (new_fd >= 0) {
                if (g_diag_fd >= 0) { close(g_diag_fd); }
                g_diag_fd = new_fd;
                (void)set_nonblocking(g_diag_fd);
                epoll_add(g_diag_fd);
                fprintf(stderr, "[DIAG] Accepted diag client fd=%d\n", g_diag_fd);
            }
            continue;
        }

        /* Data on existing connections */
        if (events[i].events & EPOLLIN) {
            if (fd == g_diag_fd) {
                process_diag_data(fd);
            } else {
                process_received_data(fd);
            }
        }
    }
    return SENTINEL_OK;
}

sentinel_err_t transport_send_command(const uint8_t *frame, size_t len)
{
    if (g_cmd_fd < 0) {
        return SENTINEL_ERR_COMM;
    }

    /* Write loop to handle partial writes */
    size_t total_written = 0;
    while (total_written < len) {
        ssize_t n = write(g_cmd_fd, frame + total_written, len - total_written);
        if (n < 0) {
            if (errno == EINTR) {
                continue; /* Retry on interrupt */
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; /* Non-blocking socket: retry */
            }
            return SENTINEL_ERR_COMM;
        }
        if (n == 0) {
            return SENTINEL_ERR_COMM; /* Connection closed */
        }
        total_written += (size_t)n;
    }
    return SENTINEL_OK;
}

void transport_set_recv_callback(transport_recv_cb_t cb, void *ctx)
{
    g_recv_cb = cb;
    g_recv_ctx = ctx;
}

void transport_set_diag_callback(transport_diag_cb_t cb, void *ctx)
{
    g_diag_cb = cb;
    g_diag_ctx = ctx;
}

static void process_diag_data(int fd)
{
    ssize_t n = read(fd, g_diag_rx_buf + g_diag_rx_pos,
                     sizeof(g_diag_rx_buf) - g_diag_rx_pos - 1U);
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            /* Client disconnected — close fd and remove from epoll */
            (void)epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            if (fd == g_diag_fd) { g_diag_fd = -1; }
            g_diag_rx_pos = 0U;
        }
        return;
    }
    g_diag_rx_pos += (size_t)n;
    g_diag_rx_buf[g_diag_rx_pos] = '\0';

    /* Process complete lines */
    char *newline = NULL;
    while ((newline = strchr((char *)g_diag_rx_buf, '\n')) != NULL) {
        *newline = '\0';
        /* Strip \r if present */
        if (newline > (char *)g_diag_rx_buf && *(newline - 1) == '\r') {
            *(newline - 1) = '\0';
        }

        if (g_diag_cb != NULL && g_diag_rx_buf[0] != '\0') {
            g_diag_cb(fd, (const char *)g_diag_rx_buf, g_diag_ctx);
        }

        /* Shift remaining data */
        size_t consumed = (size_t)(newline - (char *)g_diag_rx_buf) + 1U;
        if (g_diag_rx_pos > consumed) {
            (void)memmove(g_diag_rx_buf, newline + 1, g_diag_rx_pos - consumed);
        }
        g_diag_rx_pos -= consumed;
        g_diag_rx_buf[g_diag_rx_pos] = '\0';
    }
}

bool transport_is_connected(void)
{
    return (g_cmd_fd >= 0);
}

void transport_close(void)
{
    if (g_cmd_fd >= 0) { close(g_cmd_fd); g_cmd_fd = -1; }
    if (g_tel_fd >= 0) { close(g_tel_fd); g_tel_fd = -1; }
    if (g_tel_listen_fd >= 0) { close(g_tel_listen_fd); g_tel_listen_fd = -1; }
    if (g_diag_fd >= 0) { close(g_diag_fd); g_diag_fd = -1; }
    if (g_diag_listen_fd >= 0) { close(g_diag_listen_fd); g_diag_listen_fd = -1; }
    if (g_epoll_fd >= 0) { close(g_epoll_fd); g_epoll_fd = -1; }
}
