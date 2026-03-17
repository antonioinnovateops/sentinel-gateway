cmake_minimum_required(VERSION 3.20)
project(sentinel-gateway
    VERSION 1.0.0
    DESCRIPTION "Sentinel Gateway — Dual-processor embedded system"
    LANGUAGES C
)

# Build options
option(BUILD_LINUX "Build Linux gateway application" ON)
option(BUILD_MCU "Build MCU firmware" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_MISRA "Enable MISRA C:2012 checks" OFF)

# Common settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warning flags (MISRA-friendly)
set(COMMON_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wformat=2
    -Wundef
    -Wno-unused-parameter
)

# Version info embedded in binaries
configure_file(
    ${CMAKE_SOURCE_DIR}/config/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

# Protobuf generated sources location
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/src/proto)

# ─────────────────────────────────────────────
# Linux Gateway Application
# ─────────────────────────────────────────────
if(BUILD_LINUX)
    add_subdirectory(src/linux)
endif()

# ─────────────────────────────────────────────
# MCU Firmware
# ─────────────────────────────────────────────
if(BUILD_MCU)
    add_subdirectory(src/mcu)
endif()

# ─────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
