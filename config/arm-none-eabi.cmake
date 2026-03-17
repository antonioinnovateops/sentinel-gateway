# CMake toolchain file for ARM Cortex-M33 (STM32U575)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=config/arm-none-eabi.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

# CPU flags for Cortex-M33 (STM32U575)
set(CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")

# C flags
set(CMAKE_C_FLAGS "${CPU_FLAGS} -fdata-sections -ffunction-sections -fno-common" CACHE STRING "")
set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -DDEBUG" CACHE STRING "")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -Wl,--print-memory-usage -specs=nosys.specs -specs=nano.specs" CACHE STRING "")

# Don't try to compile test programs (cross-compilation)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
