/**
 * @file tcp_stack_qemu.c
 * @brief FW-04: TCP communication using POSIX sockets for QEMU user-mode
 *
 * This implementation replaces the lwIP-based tcp_stack.c when building
 * the MCU firmware for QEMU user-mode emulation (qemu-arm-static).
 *
 * Unlike the bare-metal version, this uses standard POSIX sockets since
 * user-mode QEMU provides direct access to host networking.
 *
 * Ports:
 * - Listen on 5000 for command channel (gateway connects to us)
 * - Connect to localhost:5001 for telemetry (we connect to gateway)
 */

#include "tcp_stack.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

/* Connection state */
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_LISTENING,
    CONN_CONNECTING,
    CONN_CONNECTED
} conn_state_t;

/* Socket descriptors */
static int g_cmd_listen_fd = -1;   /* Listening socket for commands */
static int g_cmd_client_fd = -1;   /* Accepted client connection */
static int g_tel_fd = -1;          /* Telemetry connection to gateway */

static conn_state_t g_cmd_state = CONN_DISCONNECTED;
static conn_state_t g_tel_state = CONN_DISCONNECTED;
static tcp_recv_cb_t g_command_callback = NULL;

/* RX buffer for command channel (accumulate partial frames) */
static uint8_t g_rx_buf[WIRE_FRAME_MAX_SIZE];
static size_t g_rx_len = 0U;

/* Telemetry target (localhost:5001 by default) */
static struct sockaddr_in g_tel_addr;

/**
 * Set socket to non-blocking mode
 */
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Set TCP_NODELAY to disable Nagle's algorithm
 */
static void set_nodelay(int fd)
{
    int flag = 1;
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

/**
 * Create and bind the command listening socket (port 5000)
 */
static sentinel_err_t setup_command_listener(void)
{
    g_cmd_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_cmd_listen_fd < 0) {
        fprintf(stderr, "[TCP-QEMU] socket() failed: %s\n", strerror(errno));
        return SENTINEL_ERR_COMM;
    }

    /* Allow address reuse */
    int reuse = 1;
    (void)setsockopt(g_cmd_listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SENTINEL_PORT_COMMAND);

    if (bind(g_cmd_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[TCP-QEMU] bind(5000) failed: %s\n", strerror(errno));
        close(g_cmd_listen_fd);
        g_cmd_listen_fd = -1;
        return SENTINEL_ERR_COMM;
    }

    if (listen(g_cmd_listen_fd, 1) < 0) {
        fprintf(stderr, "[TCP-QEMU] listen() failed: %s\n", strerror(errno));
        close(g_cmd_listen_fd);
        g_cmd_listen_fd = -1;
        return SENTINEL_ERR_COMM;
    }

    set_nonblocking(g_cmd_listen_fd);
    fprintf(stderr, "[TCP-QEMU] Listening on port %u for commands\n", SENTINEL_PORT_COMMAND);

    g_cmd_state = CONN_LISTENING;
    return SENTINEL_OK;
}

/**
 * Set up telemetry connection target address
 */
static void setup_telemetry_target(void)
{
    memset(&g_tel_addr, 0, sizeof(g_tel_addr));
    g_tel_addr.sin_family = AF_INET;
    g_tel_addr.sin_port = htons(SENTINEL_PORT_TELEMETRY);
    /* Connect to localhost (gateway runs on same host in user-mode QEMU) */
    g_tel_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    g_tel_state = CONN_CONNECTING;
}

/**
 * Attempt to connect to the telemetry endpoint
 */
static void try_telemetry_connect(void)
{
    if (g_tel_fd >= 0) {
        return; /* Already have a socket */
    }

    g_tel_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_tel_fd < 0) {
        return;
    }

    set_nonblocking(g_tel_fd);

    int ret = connect(g_tel_fd, (struct sockaddr *)&g_tel_addr, sizeof(g_tel_addr));
    if (ret == 0) {
        /* Immediate success (unlikely for non-blocking) */
        set_nodelay(g_tel_fd);
        g_tel_state = CONN_CONNECTED;
        fprintf(stderr, "[TCP-QEMU] Connected to telemetry port %u\n", SENTINEL_PORT_TELEMETRY);
    } else if (errno == EINPROGRESS) {
        /* Connection in progress - will check in poll */
        g_tel_state = CONN_CONNECTING;
    } else {
        /* Failed */
        close(g_tel_fd);
        g_tel_fd = -1;
    }
}

/**
 * Check if non-blocking connect completed
 */
static void check_telemetry_connect(void)
{
    if (g_tel_fd < 0 || g_tel_state != CONN_CONNECTING) {
        return;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(g_tel_fd, &wfds);

    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    int ret = select(g_tel_fd + 1, NULL, &wfds, NULL, &tv);

    if (ret > 0 && FD_ISSET(g_tel_fd, &wfds)) {
        /* Check if connect succeeded */
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(g_tel_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
            set_nodelay(g_tel_fd);
            g_tel_state = CONN_CONNECTED;
            fprintf(stderr, "[TCP-QEMU] Connected to telemetry port %u\n", SENTINEL_PORT_TELEMETRY);
        } else {
            /* Connect failed */
            close(g_tel_fd);
            g_tel_fd = -1;
            g_tel_state = CONN_CONNECTING;
        }
    }
}

/**
 * Accept incoming command connection
 */
static void accept_command_connection(void)
{
    if (g_cmd_listen_fd < 0 || g_cmd_client_fd >= 0) {
        return; /* No listener or already have a client */
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(g_cmd_listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "[TCP-QEMU] accept() failed: %s\n", strerror(errno));
        }
        return;
    }

    set_nonblocking(client_fd);
    set_nodelay(client_fd);
    g_cmd_client_fd = client_fd;
    g_cmd_state = CONN_CONNECTED;
    g_rx_len = 0U;

    fprintf(stderr, "[TCP-QEMU] Command client connected from %s:%u\n",
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}

/**
 * Read data from command socket and dispatch complete frames
 */
static void process_command_data(void)
{
    if (g_cmd_client_fd < 0 || g_cmd_state != CONN_CONNECTED) {
        return;
    }

    /* Read available data */
    ssize_t n = recv(g_cmd_client_fd, g_rx_buf + g_rx_len,
                     sizeof(g_rx_buf) - g_rx_len, 0);

    if (n > 0) {
        g_rx_len += (size_t)n;

        /* Process complete frames (header = 4-byte length + 1-byte type = 5 bytes) */
        while (g_rx_len >= WIRE_FRAME_HEADER_SIZE) {
            /* Extract payload length from header (little-endian) */
            uint32_t payload_len = (uint32_t)g_rx_buf[0]
                                 | ((uint32_t)g_rx_buf[1] << 8)
                                 | ((uint32_t)g_rx_buf[2] << 16)
                                 | ((uint32_t)g_rx_buf[3] << 24);

            size_t frame_len = WIRE_FRAME_HEADER_SIZE + payload_len;

            if (payload_len > WIRE_FRAME_MAX_PAYLOAD) {
                /* Invalid frame - reset buffer */
                fprintf(stderr, "[TCP-QEMU] Invalid frame length %u, resetting\n", payload_len);
                g_rx_len = 0U;
                break;
            }

            if (g_rx_len < frame_len) {
                /* Need more data */
                break;
            }

            /* Complete frame - dispatch to callback */
            if (g_command_callback != NULL) {
                g_command_callback(g_rx_buf, frame_len);
            }

            /* Shift remaining data */
            if (g_rx_len > frame_len) {
                memmove(g_rx_buf, g_rx_buf + frame_len, g_rx_len - frame_len);
            }
            g_rx_len -= frame_len;
        }
    } else if (n == 0) {
        /* Client disconnected */
        fprintf(stderr, "[TCP-QEMU] Command client disconnected\n");
        close(g_cmd_client_fd);
        g_cmd_client_fd = -1;
        g_cmd_state = CONN_LISTENING;
        g_rx_len = 0U;
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "[TCP-QEMU] recv() error: %s\n", strerror(errno));
            close(g_cmd_client_fd);
            g_cmd_client_fd = -1;
            g_cmd_state = CONN_LISTENING;
            g_rx_len = 0U;
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

sentinel_err_t tcp_stack_init(const system_config_t *config)
{
    (void)config;

    /* Reset state */
    g_cmd_listen_fd = -1;
    g_cmd_client_fd = -1;
    g_tel_fd = -1;
    g_cmd_state = CONN_DISCONNECTED;
    g_tel_state = CONN_DISCONNECTED;
    g_rx_len = 0U;

    fprintf(stderr, "[TCP-QEMU] Initializing POSIX socket TCP stack\n");

    /* Set up command listener */
    sentinel_err_t err = setup_command_listener();
    if (err != SENTINEL_OK) {
        return err;
    }

    /* Set up telemetry target */
    setup_telemetry_target();

    return SENTINEL_OK;
}

void tcp_stack_poll(void)
{
    /* Accept new command connections */
    accept_command_connection();

    /* Process incoming command data */
    process_command_data();

    /* Handle telemetry connection */
    if (g_tel_state == CONN_CONNECTING) {
        if (g_tel_fd < 0) {
            try_telemetry_connect();
        } else {
            check_telemetry_connect();
        }
    }
}

sentinel_err_t tcp_stack_send_telemetry(const uint8_t *frame, size_t len)
{
    if (frame == NULL || len == 0U) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    if (g_tel_fd < 0 || g_tel_state != CONN_CONNECTED) {
        /* Try to reconnect */
        if (g_tel_fd >= 0) {
            close(g_tel_fd);
            g_tel_fd = -1;
        }
        g_tel_state = CONN_CONNECTING;
        return SENTINEL_ERR_COMM;
    }

    ssize_t sent = send(g_tel_fd, frame, len, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            fprintf(stderr, "[TCP-QEMU] Telemetry connection lost\n");
            close(g_tel_fd);
            g_tel_fd = -1;
            g_tel_state = CONN_CONNECTING;
        }
        return SENTINEL_ERR_COMM;
    }

    return SENTINEL_OK;
}

sentinel_err_t tcp_stack_send_command_response(const uint8_t *frame, size_t len)
{
    if (frame == NULL || len == 0U) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    if (g_cmd_client_fd < 0 || g_cmd_state != CONN_CONNECTED) {
        return SENTINEL_ERR_COMM;
    }

    ssize_t sent = send(g_cmd_client_fd, frame, len, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            fprintf(stderr, "[TCP-QEMU] Command client connection lost\n");
            close(g_cmd_client_fd);
            g_cmd_client_fd = -1;
            g_cmd_state = CONN_LISTENING;
        }
        return SENTINEL_ERR_COMM;
    }

    return SENTINEL_OK;
}

bool tcp_stack_is_connected(void)
{
    /* Consider connected if command channel is up */
    /* Telemetry may lag behind - MCU can still function */
    return (g_cmd_state == CONN_CONNECTED);
}

void tcp_stack_register_command_callback(tcp_recv_cb_t callback)
{
    g_command_callback = callback;
}

sentinel_err_t tcp_stack_reconnect(void)
{
    /* Close existing connections */
    if (g_cmd_client_fd >= 0) {
        close(g_cmd_client_fd);
        g_cmd_client_fd = -1;
    }
    if (g_tel_fd >= 0) {
        close(g_tel_fd);
        g_tel_fd = -1;
    }

    g_cmd_state = CONN_LISTENING;
    g_tel_state = CONN_CONNECTING;
    g_rx_len = 0U;

    return SENTINEL_OK;
}
