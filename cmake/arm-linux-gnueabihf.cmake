# CMake Toolchain File for ARM Linux (user-mode, for QEMU qemu-arm-static)
#
# This toolchain builds for Linux ARM (armhf), not bare-metal.
# The resulting ELF can run under qemu-arm-static with full POSIX support.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Cross-compiler
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_AR arm-linux-gnueabihf-ar)
set(CMAKE_RANLIB arm-linux-gnueabihf-ranlib)
set(CMAKE_STRIP arm-linux-gnueabihf-strip)

# Compiler flags for ARM Cortex-A compatible (works with qemu-arm-static)
# Note: We use ARMv7-A which is widely compatible with QEMU user-mode
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16")
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -ffunction-sections -fdata-sections")

# Link options - use standard libc, not bare-metal newlib
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

# Static linking for portability in Docker/QEMU
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -static")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Define that we're building for QEMU user-mode
add_compile_definitions(SENTINEL_QEMU_USERMODE=1)
