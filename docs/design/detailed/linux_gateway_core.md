---
title: "Detailed Design — Linux Gateway Core"
document_id: "DD-LINUX-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "LINUX / gateway_core"
requirements: ["SWE-046-1", "SWE-044-1", "SWE-066-1", "SWE-057-1"]
---

# Detailed Design — Linux Gateway Core

## 1. Purpose

The gateway_core module is the main entry point for the Linux gateway application (`sentinel-gw` daemon). It owns the epoll event loop, startup/shutdown sequence, and coordinates all other modules.

## 2. Process Architecture

```
sentinel-gw (single process)
├── Main Thread
│   ├── epoll event loop
│   ├── TCP I/O handling
│   ├── Message dispatch
│   └── Signal handling
│
└── Logger Thread
    ├── Dequeue from ring buffer
    ├── Write to log files
    └── fsync every 5 seconds
```

## 3. Startup Sequence

```c
/**
 * @brief Gateway main entry point
 * @implements [SWE-066-1] Gateway Startup Sequence
 */
int main(int argc, char *argv[])
{
    /* [1] Parse command-line args and config file */
    gateway_config_t config;
    parse_config(argc, argv, &config);

    /* [2] Initialize logger (must be first for error reporting) */
    logger_init(config.log_dir, config.log_level);
    logger_event(LOG_INFO, "main", "Sentinel Gateway v%s starting", SENTINEL_VERSION_STRING);

    /* [3] Install signal handlers */
    install_signal_handlers();  /* SIGTERM, SIGINT → graceful shutdown */

    /* [4] Initialize transport (open server sockets) */
    transport_init(&config.transport);
    /* TCP:5001 server listening (telemetry from MCU) */
    /* TCP:5002 server listening (diagnostic clients) */

    /* [5] Initialize diagnostic server */
    diag_init(&g_diag_ctx);

    /* [6] Wait for MCU USB enumeration */
    logger_event(LOG_INFO, "main", "Waiting for MCU USB device...");
    wait_for_usb_device("usb0", 30);  /* timeout: 30 seconds */

    /* [7] Configure usb0 network interface */
    configure_network_interface("usb0", "192.168.7.1", "255.255.255.0");

    /* [8] Connect to MCU command port */
    transport_connect_mcu(config.mcu_ip, config.mcu_cmd_port);

    /* [9] Request state synchronization */
    state_sync_request();  /* SWE-044-1 */

    /* [10] Query MCU version */
    query_mcu_version();  /* SWE-057-1 */

    logger_event(LOG_INFO, "main", "Initialization complete, entering main loop");

    /* [11] Main event loop */
    event_loop();

    /* [12] Graceful shutdown */
    logger_event(LOG_INFO, "main", "Shutting down");
    transport_shutdown();
    logger_shutdown();

    return 0;
}
```

## 4. Event Loop Design

```c
/**
 * @brief Main epoll event loop
 * @implements [SWE-046-1]
 */
static void event_loop(void)
{
    int epoll_fd = epoll_create1(0);
    struct epoll_event events[MAX_EVENTS];

    /* Register file descriptors */
    transport_register_epoll(epoll_fd);  /* TCP sockets */
    diag_register_epoll(epoll_fd);       /* Diagnostic server */

    /* Timer fd for periodic tasks (1 second) */
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec timer_spec = {
        .it_interval = { .tv_sec = 1, .tv_nsec = 0 },
        .it_value    = { .tv_sec = 1, .tv_nsec = 0 },
    };
    timerfd_settime(timer_fd, 0, &timer_spec, NULL);
    epoll_register(epoll_fd, timer_fd, EPOLLIN);

    g_running = true;

    while (g_running)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000 /*ms timeout*/);

        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;

            if (fd == timer_fd)
            {
                /* Periodic tasks */
                uint64_t expirations;
                (void)read(timer_fd, &expirations, sizeof(expirations));

                health_monitor_check();    /* Check MCU heartbeat */
                logger_periodic_flush();   /* Flush logs if needed */
            }
            else
            {
                /* TCP events */
                transport_handle_event(&events[i]);
                diag_handle_event(&events[i]);
            }
        }
    }

    close(timer_fd);
    close(epoll_fd);
}
```

## 5. Message Dispatch

When tcp_transport receives a complete wire frame, it dispatches based on message type:

| Message Type | Handler Module | Action |
|-------------|----------------|--------|
| 0x01 SensorData | sensor_proxy | Decode, cache, log |
| 0x02 HealthStatus | health_monitor | Update state, reset timeout |
| 0x11 ActuatorResponse | actuator_proxy | Match to pending command |
| 0x21 ConfigResponse | config_manager | Update config cache |
| 0x31 DiagResponse | gateway_core | Cache MCU version/config |
| 0x41 StateSyncResponse | gateway_core | Update all caches |

## 6. Signal Handling

```c
static volatile sig_atomic_t g_running = 1;

static void signal_handler(int signum)
{
    if ((signum == SIGTERM) || (signum == SIGINT))
    {
        g_running = 0;  /* Signal event loop to exit */
    }
}
```

## 7. Error Recovery

| Error | Recovery Action |
|-------|----------------|
| MCU TCP disconnect | health_monitor: reconnect with backoff |
| MCU heartbeat timeout | health_monitor: USB power cycle |
| Diagnostic client disconnect | diagnostics: close fd, free slot |
| Protobuf decode error | Log warning, skip message |
| Log write failure | Retry once, disable logging on persistent failure |
| SIGTERM/SIGINT | Set g_running=0, graceful shutdown |
