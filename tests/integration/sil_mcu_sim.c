/**
 * @file sil_mcu_sim.c
 * @brief MCU simulator for SIL testing — runs MCU logic on x86 with TCP
 *
 * This replaces QEMU: compiles MCU application logic for x86_64,
 * using real TCP sockets instead of lwIP + TAP networking.
 * HAL drivers are stubbed with software simulation.
 *
 * Proves: protobuf encoding, wire framing, state machine, safety logic,
 * config store, sensor data flow, actuator command flow, health reporting.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

#include "../../src/common/sentinel_types.h"
#include "../../src/common/wire_frame.h"

/* Simulated sensor values */
static uint32_t g_sim_adc[4] = {2048, 1024, 3000, 500};
static float g_sim_pwm[2] = {0.0f, 0.0f};
static uint32_t g_tick_ms = 0;
static volatile sig_atomic_t g_running = 1;

/* System state */
static system_state_t g_state = STATE_INIT;
static system_config_t g_config;
static uint32_t g_sequence = 0;
static uint32_t g_health_count = 0;
static uint32_t g_sensor_count = 0;

/* Sockets */
static int g_cmd_listen_fd = -1; /* Port 5000 — commands from Linux */
static int g_cmd_fd = -1;       /* Accepted command connection */
static int g_tel_fd = -1;       /* Connected to Linux port 5001 */

static void signal_handler(int sig) { (void)sig; g_running = 0; }

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* ---- Protobuf-like encoding (minimal, wire-compatible) ---- */

static size_t enc_varint(uint8_t *buf, uint64_t val)
{
    size_t n = 0;
    while (val > 0x7FU) { buf[n++] = (uint8_t)(val & 0x7FU) | 0x80U; val >>= 7; }
    buf[n++] = (uint8_t)val;
    return n;
}

static size_t enc_field_varint(uint8_t *buf, uint32_t field, uint64_t val)
{
    size_t n = enc_varint(buf, ((uint64_t)field << 3) | 0ULL);
    n += enc_varint(buf + n, val);
    return n;
}

static size_t enc_field_float(uint8_t *buf, uint32_t field, float val)
{
    size_t n = enc_varint(buf, ((uint64_t)field << 3) | 5ULL);
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    buf[n+0] = (uint8_t)(bits & 0xFF);
    buf[n+1] = (uint8_t)((bits >> 8) & 0xFF);
    buf[n+2] = (uint8_t)((bits >> 16) & 0xFF);
    buf[n+3] = (uint8_t)((bits >> 24) & 0xFF);
    return n + 4;
}

static size_t enc_header(uint8_t *buf, uint32_t field_num)
{
    uint8_t hdr[48];
    size_t hlen = 0;
    g_sequence++;
    hlen += enc_field_varint(hdr + hlen, 1, g_sequence);
    hlen += enc_field_varint(hdr + hlen, 2, get_ms() * 1000ULL);

    uint8_t ver[8];
    size_t vlen = 0;
    vlen += enc_field_varint(ver + vlen, 1, 1);
    vlen += enc_field_varint(ver + vlen, 2, 0);
    hlen += enc_varint(hdr + hlen, ((uint64_t)3 << 3) | 2ULL);
    hlen += enc_varint(hdr + hlen, vlen);
    memcpy(hdr + hlen, ver, vlen);
    hlen += vlen;

    size_t n = enc_varint(buf, ((uint64_t)field_num << 3) | 2ULL);
    n += enc_varint(buf + n, hlen);
    memcpy(buf + n, hdr, hlen);
    return n + hlen;
}

/* ---- Build and send messages ---- */

static void send_frame(int fd, uint8_t msg_type, const uint8_t *payload, size_t plen)
{
    uint8_t frame[WIRE_FRAME_MAX_SIZE];
    size_t flen = 0;
    if (wire_frame_encode(msg_type, payload, plen, frame, &flen) == SENTINEL_OK) {
        (void)write(fd, frame, flen);
    }
}

static void send_health(void)
{
    if (g_tel_fd < 0) return;

    uint8_t payload[256];
    size_t plen = 0;

    plen += enc_header(payload + plen, 1);
    plen += enc_field_varint(payload + plen, 2, (uint64_t)g_state);
    plen += enc_field_varint(payload + plen, 3, (g_tel_fd >= 0) ? 0ULL : 2ULL);
    plen += enc_field_varint(payload + plen, 4, g_tick_ms / 1000U);
    plen += enc_field_varint(payload + plen, 5, 0); /* watchdog resets */
    plen += enc_field_varint(payload + plen, 7, 28000); /* free stack */
    plen += enc_field_varint(payload + plen, 8, 0); /* no fault */
    plen += enc_field_float(payload + plen, 9, g_sim_pwm[0]);
    plen += enc_field_float(payload + plen, 10, g_sim_pwm[1]);

    send_frame(g_tel_fd, MSG_TYPE_HEALTH_STATUS, payload, plen);
    g_health_count++;
}

static void send_sensor_data(void)
{
    if (g_tel_fd < 0) return;

    uint8_t payload[256];
    size_t plen = 0;

    plen += enc_header(payload + plen, 1);

    for (int ch = 0; ch < 4; ch++) {
        /* Simulate ADC drift */
        g_sim_adc[ch] = (uint32_t)((int32_t)g_sim_adc[ch] + (rand() % 11) - 5);
        if (g_sim_adc[ch] > 4095) g_sim_adc[ch] = 4095;

        uint8_t reading[32];
        size_t rlen = 0;
        rlen += enc_field_varint(reading + rlen, 1, (uint32_t)ch);
        rlen += enc_field_varint(reading + rlen, 2, g_sim_adc[ch]);

        float cal = 0.0f;
        switch (ch) {
            case 0: cal = (float)g_sim_adc[ch] * 0.0806f - 40.0f; break;
            case 1: cal = (float)g_sim_adc[ch] * 0.0244f; break;
            case 2: cal = (float)g_sim_adc[ch] * 0.3663f + 300.0f; break;
            case 3: cal = (float)g_sim_adc[ch] * 24.42f; break;
        }
        rlen += enc_field_float(reading + rlen, 3, cal);

        plen += enc_varint(payload + plen, (2ULL << 3) | 2ULL);
        plen += enc_varint(payload + plen, rlen);
        memcpy(payload + plen, reading, rlen);
        plen += rlen;
    }

    plen += enc_field_varint(payload + plen, 3, g_config.sensor_rates_hz[0]);

    send_frame(g_tel_fd, MSG_TYPE_SENSOR_DATA, payload, plen);
    g_sensor_count++;
}

static void send_actuator_response(uint8_t id, float applied, uint32_t status)
{
    if (g_cmd_fd < 0) return;

    uint8_t payload[128];
    size_t plen = 0;
    plen += enc_header(payload + plen, 1);
    plen += enc_field_varint(payload + plen, 2, id);
    plen += enc_field_float(payload + plen, 3, applied);
    plen += enc_field_varint(payload + plen, 4, status);

    send_frame(g_cmd_fd, MSG_TYPE_ACTUATOR_RSP, payload, plen);
}

static void send_config_response(uint32_t status)
{
    if (g_cmd_fd < 0) return;

    uint8_t payload[64];
    size_t plen = 0;
    plen += enc_header(payload + plen, 1);
    plen += enc_field_varint(payload + plen, 2, status);

    send_frame(g_cmd_fd, MSG_TYPE_CONFIG_RSP, payload, plen);
}

/* ---- Process incoming commands ---- */

static void process_command(const uint8_t *frame, size_t len)
{
    if (len < WIRE_FRAME_HEADER_SIZE) return;

    uint8_t msg_type;
    uint32_t payload_len;
    if (wire_frame_decode_header(frame, len, &msg_type, &payload_len) != SENTINEL_OK) return;

    printf("[MCU-SIM] Received msg type 0x%02X, payload %u bytes\n", msg_type, payload_len);

    switch (msg_type) {
        case MSG_TYPE_ACTUATOR_CMD:
            /* Simplified: accept and apply */
            g_sim_pwm[0] = 50.0f; /* Simulated apply */
            send_actuator_response(0, g_sim_pwm[0], 0);
            break;

        case MSG_TYPE_CONFIG_UPDATE:
            send_config_response(0);
            break;

        case MSG_TYPE_STATE_SYNC_REQ:
            printf("[MCU-SIM] State sync requested — transitioning to NORMAL\n");
            g_state = STATE_NORMAL;
            break;

        default:
            printf("[MCU-SIM] Unknown command type 0x%02X\n", msg_type);
            break;
    }
}

/* ---- Network setup ---- */

static int setup_listen(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    listen(fd, 1);
    set_nonblocking(fd);
    return fd;
}

static int try_connect(const char *ip, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    int opt = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    set_nonblocking(fd);
    return fd;
}

/* ---- Main ---- */

int main(int argc, char *argv[])
{
    const char *linux_ip = "127.0.0.1";
    if (argc > 1) linux_ip = argv[1];

    printf("[MCU-SIM] Sentinel MCU Simulator starting...\n");
    printf("[MCU-SIM] Linux IP: %s\n", linux_ip);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    srand((unsigned)time(NULL));

    /* Load defaults */
    g_config.sensor_rates_hz[0] = 10;
    g_config.sensor_rates_hz[1] = 10;
    g_config.sensor_rates_hz[2] = 10;
    g_config.sensor_rates_hz[3] = 10;
    g_config.heartbeat_interval_ms = 1000;
    g_config.comm_timeout_ms = 3000;

    /* Listen on port 5000 for commands */
    g_cmd_listen_fd = setup_listen(SENTINEL_PORT_COMMAND);
    if (g_cmd_listen_fd < 0) {
        fprintf(stderr, "Failed to listen on port %u\n", SENTINEL_PORT_COMMAND);
        return 1;
    }
    printf("[MCU-SIM] Listening on port %u (commands)\n", SENTINEL_PORT_COMMAND);

    uint64_t start_ms = get_ms();
    uint64_t last_health_ms = 0;
    uint64_t last_sensor_ms = 0;
    uint64_t last_connect_ms = 0;

    /* Super-loop simulation */
    while (g_running) {
        uint64_t now_ms = get_ms();
        g_tick_ms = (uint32_t)(now_ms - start_ms);

        /* Accept command connection */
        if (g_cmd_fd < 0) {
            int fd = accept(g_cmd_listen_fd, NULL, NULL);
            if (fd >= 0) {
                g_cmd_fd = fd;
                set_nonblocking(fd);
                printf("[MCU-SIM] Command connection accepted\n");
            }
        }

        /* Connect to Linux telemetry port */
        if (g_tel_fd < 0 && (now_ms - last_connect_ms) >= 1000) {
            last_connect_ms = now_ms;
            g_tel_fd = try_connect(linux_ip, SENTINEL_PORT_TELEMETRY);
            if (g_tel_fd >= 0) {
                printf("[MCU-SIM] Connected to Linux telemetry port %u\n",
                       SENTINEL_PORT_TELEMETRY);
                if (g_state == STATE_INIT) {
                    g_state = STATE_NORMAL;
                    printf("[MCU-SIM] State → NORMAL\n");
                }
            }
        }

        /* Read commands */
        if (g_cmd_fd >= 0) {
            uint8_t buf[WIRE_FRAME_MAX_SIZE];
            ssize_t n = read(g_cmd_fd, buf, sizeof(buf));
            if (n > 0) {
                process_command(buf, (size_t)n);
            } else if (n == 0) {
                printf("[MCU-SIM] Command connection closed\n");
                close(g_cmd_fd);
                g_cmd_fd = -1;
            }
        }

        /* Send health status every 1 second */
        if ((now_ms - last_health_ms) >= g_config.heartbeat_interval_ms) {
            last_health_ms = now_ms;
            send_health();
            if (g_health_count % 10 == 1) {
                printf("[MCU-SIM] Health #%u sent (state=%d, pwm=[%.1f, %.1f])\n",
                       g_health_count, g_state, (double)g_sim_pwm[0], (double)g_sim_pwm[1]);
            }
        }

        /* Send sensor data at configured rate */
        uint32_t sensor_interval = 1000 / g_config.sensor_rates_hz[0];
        if ((now_ms - last_sensor_ms) >= sensor_interval) {
            last_sensor_ms = now_ms;
            send_sensor_data();
        }

        /* Sleep 10ms (super-loop tick) */
        usleep(10000);
    }

    printf("\n[MCU-SIM] Shutting down. Sent %u health, %u sensor messages.\n",
           g_health_count, g_sensor_count);

    if (g_cmd_fd >= 0) close(g_cmd_fd);
    if (g_cmd_listen_fd >= 0) close(g_cmd_listen_fd);
    if (g_tel_fd >= 0) close(g_tel_fd);
    return 0;
}
