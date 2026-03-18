cmake_minimum_required(VERSION 3.20)

# Unity test framework
add_library(unity STATIC unit/vendor/unity.c)
target_include_directories(unity PUBLIC unit/vendor)

# ============================================
# Test: Wire Frame
# ============================================
add_executable(test_wire_frame
    unit/test_wire_frame.c
    ${CMAKE_SOURCE_DIR}/src/common/wire_frame.c
)
target_include_directories(test_wire_frame PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)
target_link_libraries(test_wire_frame unity)
add_test(NAME wire_frame COMMAND test_wire_frame)

# ============================================
# Test: Error Codes
# ============================================
add_executable(test_error_codes
    unit/test_error_codes.c
)
target_include_directories(test_error_codes PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
)
target_link_libraries(test_error_codes unity)
add_test(NAME error_codes COMMAND test_error_codes)

# ============================================
# Test: Config Store (uses flash_driver simulation)
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
)
target_compile_definitions(test_config_store PRIVATE
    SENTINEL_PLATFORM_MCU=1
    __HEAP_SIZE=0
)
target_link_libraries(test_config_store unity)
add_test(NAME config_store COMMAND test_config_store)

# ============================================
# Test: Actuator Control (uses pwm_driver)
# ============================================
add_executable(test_actuator_control
    unit/test_actuator_control.c
    ${CMAKE_SOURCE_DIR}/src/mcu/actuator_control.c
    unit/stubs/pwm_stub.c
)
target_include_directories(test_actuator_control PRIVATE
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/mcu
    ${CMAKE_SOURCE_DIR}/src/mcu/hal
)
target_compile_definitions(test_actuator_control PRIVATE
    SENTINEL_PLATFORM_MCU=1
)
target_link_libraries(test_actuator_control unity m)
add_test(NAME actuator_control COMMAND test_actuator_control)
