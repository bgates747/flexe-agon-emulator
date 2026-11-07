cmake_minimum_required(VERSION 3.14)
project(xtensa-emulator C)
set(CMAKE_C_STANDARD 17)

# Default to Release build (enables -O3)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Profile-guided optimization (PGO)
# Usage:
#   1. cmake -B build-pgo -DPGO=generate &&
#      cmake --build build-pgo --target xtensa-emu
#   2. ./build-pgo/xtensa-emu -q -c 100000000 -s firmware.elf firmware.bin
#   3. cmake -B build-pgo -DPGO=use &&
#      cmake --build build-pgo --target xtensa-emu
set(PGO "" CACHE STRING "PGO mode: 'generate' to collect profiles, 'use' to apply them")
# Older build trees cached this string-valued setting as a BOOL because it was
# previously declared with option(). Migrate the normal disabled value in
# place so developers do not need to throw away their build directories.
if(PGO STREQUAL "OFF")
    set(PGO "" CACHE STRING "PGO mode: 'generate' to collect profiles, 'use' to apply them" FORCE)
endif()
set_property(CACHE PGO PROPERTY STRINGS "" generate use)
set(PREDECODE_FLASH_MB "4" CACHE STRING "Pre-decode table flash coverage in MB (0=disabled, 2/4/8/16)")
# -march=native bakes the building machine's CPU into the binary, which is
# what you want locally and not what you want on a shared CI runner: the
# result is unreproducible, and a runner whose feature set the compiler
# dislikes fails the build outright under -Werror for reasons that have
# nothing to do with the code.
# Interpreter sampling profiler. Off by default: the runtime flag check costs
# ~3% in the dispatch loop even when the profiler is disabled.
option(FLEXE_PROFILE "Build the interpreter sampling profiler" OFF)
if(FLEXE_PROFILE)
    add_compile_definitions(FLEXE_PROFILE_BUILD=1)
endif()

option(NATIVE_ARCH "Tune Release builds for the building machine's CPU" ON)
option(FLEXE_LTO "Enable LTO in optimized production builds" ON)
option(FLEXE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FLEXE_CCACHE "Use ccache automatically when it is available" ON)

if(FLEXE_CCACHE AND NOT MSVC AND NOT CMAKE_C_COMPILER_LAUNCHER)
    find_program(FLEXE_CCACHE_EXECUTABLE ccache)
    if(FLEXE_CCACHE_EXECUTABLE)
        # Keep the automatic choice out of the cache so FLEXE_CCACHE=OFF
        # also takes effect when an existing build tree is reconfigured.
        set(CMAKE_C_COMPILER_LAUNCHER "${FLEXE_CCACHE_EXECUTABLE}")
        message(STATUS "Compiler cache: ${FLEXE_CCACHE_EXECUTABLE}")
    endif()
endif()

if(FLEXE_SANITIZERS AND MSVC)
    message(FATAL_ERROR "FLEXE_SANITIZERS requires GCC or Clang")
endif()
if(NOT PGO STREQUAL "" AND
   NOT PGO STREQUAL "generate" AND
   NOT PGO STREQUAL "use")
    message(FATAL_ERROR "PGO must be empty, 'generate', or 'use' (got '${PGO}')")
endif()
if(FLEXE_SANITIZERS AND PGO)
    message(FATAL_ERROR "FLEXE_SANITIZERS and PGO are separate build modes")
endif()

# LTO and native tuning are independent controls for optimized GCC/Clang
# builds: disabling expensive whole-program links must not silently make the
# resulting emulator less representative of a normal local build.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR
   CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(NOT MSVC AND NATIVE_ARCH)
        add_compile_options(-march=native)
    endif()
    if(FLEXE_LTO AND NOT FLEXE_SANITIZERS)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT IPO_SUPPORTED)
        if(IPO_SUPPORTED AND NOT PGO)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()
endif()

if(FLEXE_SANITIZERS)
    message(STATUS "Sanitizers: address,undefined at -O1 (LTO disabled)")
    # RelWithDebInfo otherwise selects -O2. Sanitizer builds are substantially
    # cheaper at -O1 while retaining optimized control flow and useful stacks.
    add_compile_options(
        -fsanitize=address,undefined -fno-omit-frame-pointer -O1)
    add_link_options(-fsanitize=address,undefined)
endif()

if(PGO STREQUAL "generate")
    message(STATUS "PGO: generating profile data")
    add_compile_options(-fprofile-generate)
    add_link_options(-fprofile-generate)
    set(PGO_NOWARN "-Wno-error")
    # Disable LTO during profile generation (conflicts with PGO on some GCC versions)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
elseif(PGO STREQUAL "use")
    message(STATUS "PGO: using profile data for optimization")
    add_compile_options(-fprofile-use -fprofile-correction)
    add_link_options(-fprofile-use)
    set(PGO_NOWARN "-Wno-error")
    # Re-enable LTO for PGO use builds
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED)
    if(IPO_SUPPORTED AND FLEXE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Library (core emulator)
set(LIB_SOURCES
    src/target.c
    src/assist_debug.c
    src/xtensa.c
    src/window_accel.c
    src/profile.c
    src/xtensa_disasm.c
    src/memory.c
    src/flash_mmu.c
    src/esp32s3_extmem.c
    src/efuse.c
    src/apb_saradc.c
    src/gpio.c
    src/gdma.c
    src/i2s_v2.c
    src/lcd_cam.c
    src/rmt_v1.c
    src/io_mux.c
    src/rtc_cntl.c
    src/rtc_io.c
    src/regi2c.c
    src/radio.c
    src/sens.c
    src/touch_v2.c
    src/sensitive_memprot.c
    src/syscon_memory.c
    src/system_clock.c
    src/systimer.c
    src/timer_group.c
    src/usb_serial_jtag.c
    src/spi_mem.c
    src/firmware_scan.c
    src/loader.c
    src/rom_elf.c
    src/peripherals.c
    src/spi_display.c
    src/sx127x.c
    src/axp192.c
    src/ublox_gps.c
    src/rom_stubs.c
    src/elf_symbols.c
    src/freertos_stubs.c
    src/esp_timer_stubs.c
    src/display_stubs.c
    src/touch_stubs.c
    src/sdcard_stubs.c
    src/guest_call.c
    src/wifi_stubs.c
    src/vfs_stubs.c
    src/bt_stubs.c
    src/sha_stubs.c
    src/aes_stubs.c
    src/mpi_stubs.c
    src/flexe_session.c
    src/savestate.c
    src/hierarchical_trace.c
    src/sandbox_input.c
    src/sandbox_events.c
    src/aot.c
)

# JIT compiler: x86-64 and ARM64, non-MSVC
if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|arm64|aarch64")
    list(APPEND LIB_SOURCES src/jit.c)
    message(STATUS "JIT compiler enabled (${CMAKE_SYSTEM_PROCESSOR})")
    set(FLEXE_HAS_JIT TRUE)
else()
    list(APPEND LIB_SOURCES src/jit_stub.c)
    message(STATUS "JIT compiler disabled (requires x86-64/ARM64, non-MSVC) — using no-op stubs")
endif()

if(MSVC)
    list(APPEND LIB_SOURCES src/msvc_compat.c)
endif()

if(FLEXE_HAS_JIT)
    add_compile_definitions(FLEXE_HAS_JIT=1)
endif()

add_library(xtensa-emu-lib STATIC ${LIB_SOURCES})
target_include_directories(xtensa-emu-lib PUBLIC src/)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

if(MSVC)
    find_package(ZLIB REQUIRED)
    target_link_libraries(xtensa-emu-lib PUBLIC
        OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB Threads::Threads)
    target_compile_options(xtensa-emu-lib PRIVATE /W4 /wd4100 /wd4244 /wd4267 /MP)
    target_compile_definitions(xtensa-emu-lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(xtensa-emu-lib PUBLIC PREDECODE_FLASH_MB=${PREDECODE_FLASH_MB})
else()
    target_link_libraries(xtensa-emu-lib PUBLIC
        m z Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
    target_compile_options(xtensa-emu-lib PRIVATE
        -Wall -Wextra -Werror -Wno-unused-parameter ${PGO_NOWARN})
    target_compile_definitions(xtensa-emu-lib PUBLIC
        PREDECODE_FLASH_MB=${PREDECODE_FLASH_MB})

    # Disable stack protector on hot-path files (saves ~3-4% from canary checks)
    set_source_files_properties(src/xtensa.c src/window_accel.c src/memory.c
        src/jit.c
        PROPERTIES COMPILE_OPTIONS "-fno-stack-protector")
endif()

