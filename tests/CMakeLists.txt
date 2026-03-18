cmake_minimum_required(VERSION 3.20)

# Unity test framework
add_library(unity STATIC
    unit/vendor/unity.c
)
target_include_directories(unity PUBLIC unit/vendor)

# ============================================
# Common module tests
# ============================================
add_executable(test_wire_frame
    unit/test_wire_frame.c
    ${CMAKE_SOURCE_DIR}/src/common/wire_frame.c
)
target_include_directories(test_wire_frame PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    unit/vendor
)
target_link_libraries(test_wire_frame unity)
add_test(NAME wire_frame COMMAND test_wire_frame)

# ============================================
# MCU component tests (host-compiled stubs)
# ============================================
add_executable(test_config_store
    unit/test_config_store.c
    ${CMAKE_SOURCE_DIR}/src/mcu/config_store.c
    ${CMAKE_SOURCE_DIR}/src/mcu/hal/flash_driver.c
)
target_include_directories(test_config_store PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/mcu
    ${CMAKE_SOURCE_DIR}/src/mcu/hal
    unit/vendor
)
target_compile_definitions(test_config_store PRIVATE
    SENTINEL_PLATFORM_MCU=1
)
target_link_libraries(test_config_store unity)
add_test(NAME config_store COMMAND test_config_store)

add_executable(test_actuator_control
    unit/test_actuator_control.c
    ${CMAKE_SOURCE_DIR}/src/mcu/actuator_control.c
    unit/stubs/pwm_driver_stub.c
)
target_include_directories(test_actuator_control PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/mcu
    ${CMAKE_SOURCE_DIR}/src/mcu/hal
    unit/vendor
    unit/stubs
)
target_compile_definitions(test_actuator_control PRIVATE
    SENTINEL_PLATFORM_MCU=1
)
target_link_libraries(test_actuator_control unity m)
add_test(NAME actuator_control COMMAND test_actuator_control)

# ============================================
# Linux component tests
# ============================================
add_executable(test_diagnostics
    unit/test_diagnostics.c
    ${CMAKE_SOURCE_DIR}/src/linux/diagnostics.c
    unit/stubs/sensor_proxy_stub.c
    unit/stubs/actuator_proxy_stub.c
    unit/stubs/health_monitor_stub.c
)
target_include_directories(test_diagnostics PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/linux
    unit/vendor
    unit/stubs
)
target_compile_definitions(test_diagnostics PRIVATE
    SENTINEL_PLATFORM_LINUX=1
    SENTINEL_VERSION="1.0.0"
    _POSIX_C_SOURCE=200809L
)
target_link_libraries(test_diagnostics unity)
add_test(NAME diagnostics COMMAND test_diagnostics)

add_executable(test_logger
    unit/test_logger.c
    ${CMAKE_SOURCE_DIR}/src/linux/logger.c
)
target_include_directories(test_logger PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/linux
    unit/vendor
)
target_compile_definitions(test_logger PRIVATE
    SENTINEL_PLATFORM_LINUX=1
    _POSIX_C_SOURCE=200809L
)
target_link_libraries(test_logger unity)
add_test(NAME logger COMMAND test_logger)
