cmake_minimum_required(VERSION 3.20)
project(sentinel-gateway VERSION 1.0.0 LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# MISRA-friendly warnings
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wdouble-promotion -Wformat=2 -Wundef
    -Wstrict-prototypes -Wmissing-prototypes
    -Wredundant-decls -Wnested-externs
    -Wno-unused-parameter
)

# Build options
option(BUILD_LINUX "Build Linux gateway" ON)
option(BUILD_MCU   "Build MCU firmware" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Version header
configure_file(
    ${CMAKE_SOURCE_DIR}/config/version.h.in
    ${CMAKE_BINARY_DIR}/generated/sentinel_version.h
    @ONLY
)
include_directories(${CMAKE_BINARY_DIR}/generated)

if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# ============================================
# Common sources
# ============================================
set(COMMON_SOURCES
    src/common/wire_frame.c
)

# ============================================
# Linux Gateway (sentinel-gw)
# ============================================
if(BUILD_LINUX)
    set(LINUX_SOURCES
        src/linux/main.c
        src/linux/gateway_core.c
        src/linux/tcp_transport.c
        src/linux/sensor_proxy.c
        src/linux/actuator_proxy.c
        src/linux/health_monitor.c
        src/linux/diagnostics.c
        src/linux/config_manager.c
        src/linux/logger.c
        ${COMMON_SOURCES}
    )

    add_executable(sentinel-gw ${LINUX_SOURCES})

    target_compile_definitions(sentinel-gw PRIVATE
        SENTINEL_PLATFORM_LINUX=1
        SENTINEL_VERSION="${PROJECT_VERSION}"
        _POSIX_C_SOURCE=200809L
    )

    target_include_directories(sentinel-gw PRIVATE
        src/common
        src/linux
    )

    # Link pthread and rt for clock_gettime
    target_link_libraries(sentinel-gw PRIVATE pthread rt)

    install(TARGETS sentinel-gw RUNTIME DESTINATION bin)
endif()

# ============================================
# MCU Firmware (sentinel-mcu)
# ============================================
if(BUILD_MCU)
    set(MCU_SOURCES
        src/mcu/main.c
        src/mcu/sensor_acquisition.c
        src/mcu/actuator_control.c
        src/mcu/protobuf_handler.c
        src/mcu/tcp_stack.c
        src/mcu/watchdog_mgr.c
        src/mcu/config_store.c
        src/mcu/health_reporter.c
        src/mcu/startup_stm32u575.s
        src/mcu/hal/adc_driver.c
        src/mcu/hal/pwm_driver.c
        src/mcu/hal/flash_driver.c
        src/mcu/hal/watchdog_driver.c
        src/mcu/hal/gpio_driver.c
        src/mcu/hal/systick.c
        ${COMMON_SOURCES}
    )

    add_executable(sentinel-mcu ${MCU_SOURCES})

    target_compile_definitions(sentinel-mcu PRIVATE
        SENTINEL_PLATFORM_MCU=1
        SENTINEL_VERSION="${PROJECT_VERSION}"
        STM32U575xx=1
        __HEAP_SIZE=0
        _REENT_SMALL=1
        PB_NO_ERRMSG=1
        PB_BUFFER_ONLY=1
    )

    target_include_directories(sentinel-mcu PRIVATE
        src/common
        src/mcu
        src/mcu/hal
    )

    # Linker script
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/mcu/stm32u575.ld)
    target_link_options(sentinel-mcu PRIVATE
        -T${LINKER_SCRIPT}
        -Wl,-Map=${CMAKE_BINARY_DIR}/sentinel-mcu.map
    )

    # Post-build: generate .bin and size report
    add_custom_command(TARGET sentinel-mcu POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary sentinel-mcu sentinel-mcu.bin
        COMMAND ${CMAKE_SIZE} sentinel-mcu
        COMMENT "Generating sentinel-mcu.bin and size report"
    )
endif()

# ============================================
# Unit Tests (host build)
# ============================================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
