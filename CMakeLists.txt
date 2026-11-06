cmake_minimum_required(VERSION 3.20)
project(ki_recomp LANGUAGES CXX)

option(KI_R36S "R36S / PortMaster fullscreen 4:3, nearest filtering and diagnostic SDL2 fallback" OFF)
option(KI_R36S_MIPS_O3 "Compile generated MIPS R4600 code with aggressive -O3 for a separate R36S A/B binary" OFF)
option(KI_R36S_LTO "Experimental R36S whole-program IPO/LTO for generated MIPS and runtime (A/B only)" OFF)
option(KI_R36S_RELEASE_CLEAN "Lean R36S distributable: no runtime profilers, frame timing, dev UI or extra benchmarks" OFF)
option(KI_R36S_LEGACY_SDL_ABI "Build legacy R36S player without newer SDL2 symbols (for firmware SDL2 fallback)" OFF)
if(KI_R36S_RELEASE_CLEAN AND (NOT KI_R36S OR NOT CMAKE_BUILD_TYPE STREQUAL "Release"))
  message(FATAL_ERROR "KI_R36S_RELEASE_CLEAN requires -DKI_R36S=ON -DCMAKE_BUILD_TYPE=Release")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Original game data never lives in the repository. The extracted code image
# (see tools/extract_game_code.sh) is an input of the build.
set(KI_CODE_IMAGE "${CMAKE_SOURCE_DIR}/data/kipack-out/rom-0.bin" CACHE FILEPATH
    "KI 1 L1.5D game code image (rom-0, loaded at 0x88000000)")
set(KI_CODE_IMAGE_SHA1 "ed875b9f2a0d1bccf79ff63189c37488ee718659" CACHE STRING
    "expected SHA1 of KI_CODE_IMAGE")
set(KI_DATA_IMAGES "${CMAKE_SOURCE_DIR}/data/kipack-out/rom-1.bin@88033900;${CMAKE_SOURCE_DIR}/data/kipack-out/rom-2.bin@88033880"
    CACHE STRING "read-only data images (FILE@VRAM) the translator reads jump tables from")
set(KI_FUNCTIONS "8802d560;8802d774;880176f0;8801a1e8;8802d694;8802dc94;880274c8;8801905c;880107a8;880123d4;88011dac;88010d8c;88010d78;88010da8" CACHE STRING "semicolon-separated guest functions to recompile")
set(KI_PYTHON "${CMAKE_SOURCE_DIR}/.venv/bin/python" CACHE FILEPATH "Python with rabbitizer installed")

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-label)
endif()

# ADSP-2105 core for the DCS sound board, derived from MAME (BSD-3-Clause, see third_party/adsp2100).
add_library(adsp2105 STATIC third_party/adsp2100/adsp2105.cpp)
target_include_directories(adsp2105 PUBLIC third_party/adsp2100)
if(KI_R36S)
  # Private build-time DSP-to-native specialization. The generated include
  # may contain sound firmware opcodes and MUST remain under build-r36s/,
  # never in source control. Without the private ROM it emits only synthetic
  # ROM-free test cases.
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(KI_DSP_BOOT_ROM "${CMAKE_SOURCE_DIR}/data/roms/kinst/u10-l1" CACHE FILEPATH
      "User-supplied original KI1 u10 sound ROM; build input only")
  set(_dsp_inc "${CMAKE_BINARY_DIR}/gen_dsp/ki_adsp_native.inc")
  set(_dsp_deps "${CMAKE_SOURCE_DIR}/tools/r36s/generate_adsp_native.py")
  if(EXISTS "${KI_DSP_BOOT_ROM}")
    list(APPEND _dsp_deps "${KI_DSP_BOOT_ROM}")
  endif()
  add_custom_command(
    OUTPUT "${_dsp_inc}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/r36s/generate_adsp_native.py"
            --rom "${KI_DSP_BOOT_ROM}" --out "${_dsp_inc}"
    DEPENDS ${_dsp_deps}
    COMMENT "Compiling private KI1 ADSP-2105 opcode specializations"
    VERBATIM)
  add_custom_target(ki_adsp_native_header DEPENDS "${_dsp_inc}")
  add_dependencies(adsp2105 ki_adsp_native_header)
  set_source_files_properties(third_party/adsp2100/adsp2105.cpp
      PROPERTIES OBJECT_DEPENDS "${_dsp_inc}")
  target_include_directories(adsp2105 PRIVATE "${CMAKE_BINARY_DIR}/gen_dsp")
  target_compile_definitions(adsp2105 PRIVATE KI_ADSP_NATIVE_IMAGE=1)
endif()
if(NOT MSVC)
  target_compile_options(adsp2105 PRIVATE -Wno-sign-compare -O2)
endif()

add_library(ki_runtime STATIC runtime/src/runtime.cpp runtime/src/kinst_board.cpp runtime/src/dcs.cpp)
target_include_directories(ki_runtime PUBLIC runtime/include)
target_link_libraries(ki_runtime PUBLIC adsp2105)
if(KI_R36S)
  # Only handheld builds enable KI-specific DSP polling acceleration by
  # default. Other platforms keep the interpreter until validated.
  target_compile_definitions(ki_runtime PRIVATE KI_R36S_FAST_DSP=1 KI_R36S_FAST_DISPATCH=1)
  if(KI_R36S_RELEASE_CLEAN)
    target_compile_definitions(ki_runtime PRIVATE KI_R36S_RELEASE_CLEAN=1)
  endif()
endif()

# A recompiled-code library defines the function table (ki::kFunctionTable) that
# the runtime looks entries up in, so the two static libraries depend on each
# other. Single-pass linkers (GNU ld) need the code library again after the
# runtime; Apple's linker does not mind.
function(ki_code_library target)
  target_link_libraries(${target} PUBLIC ki_runtime)
  if(NOT APPLE)
    target_link_libraries(${target} INTERFACE "$<TARGET_FILE:${target}>")
  endif()
endfunction()

set(GEN_DIR "${CMAKE_BINARY_DIR}/gen")
set(GEN_SRC "${GEN_DIR}/recompiled.cpp")
set(func_args "")
foreach(f IN LISTS KI_FUNCTIONS)
  list(APPEND func_args --func ${f})
endforeach()
set(data_deps "")
foreach(d IN LISTS KI_DATA_IMAGES)
  list(APPEND func_args --data ${d})
  string(REGEX REPLACE "@[0-9a-fA-F]+$" "" d_file "${d}")
  list(APPEND data_deps "${d_file}")
endforeach()
add_custom_command(
  OUTPUT "${GEN_SRC}" "${GEN_DIR}/recompiled.lst"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GEN_DIR}"
  COMMAND "${KI_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py"
          --image "${KI_CODE_IMAGE}" --vram 88000000 --expect-sha1 ${KI_CODE_IMAGE_SHA1}
          ${func_args} --no-timing --out "${GEN_SRC}" --listing "${GEN_DIR}/recompiled.lst"
  DEPENDS "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py" "${KI_CODE_IMAGE}" ${data_deps}
  COMMENT "Recompiling guest functions: ${KI_FUNCTIONS}"
  VERBATIM)

add_library(ki_recompiled STATIC "${GEN_SRC}")
ki_code_library(ki_recompiled)

add_executable(compare_case tests/compare_case.cpp)
target_link_libraries(compare_case PRIVATE ki_recompiled)

# Runtime self-test with synthetic values only (no game data needed).
add_executable(runtime_selftest tests/runtime_selftest.cpp tests/empty_function_table.cpp)
target_link_libraries(runtime_selftest PRIVATE ki_runtime)

enable_testing()
add_test(NAME runtime_selftest COMMAND runtime_selftest)

# ROM-free differential verification of the ADSP2105 fast bus map.
add_executable(dcs_fastmem_selftest tests/dcs_fastmem_selftest.cpp)
target_link_libraries(dcs_fastmem_selftest PRIVATE ki_runtime)
add_test(NAME dcs_fastmem_selftest COMMAND dcs_fastmem_selftest)

add_executable(dsp_hotspot_selftest tests/dsp_hotspot_selftest.cpp)
target_link_libraries(dsp_hotspot_selftest PRIVATE adsp2105)
add_test(NAME dsp_hotspot_selftest COMMAND dsp_hotspot_selftest)

add_executable(dsp_waitloop_selftest tests/dsp_waitloop_selftest.cpp)
target_link_libraries(dsp_waitloop_selftest PRIVATE adsp2105)
add_test(NAME dsp_waitloop_selftest COMMAND dsp_waitloop_selftest)

if(KI_R36S)
  add_executable(adsp_native_selftest tests/adsp_native_selftest.cpp)
  target_link_libraries(adsp_native_selftest PRIVATE adsp2105)
  add_test(NAME adsp_native_selftest COMMAND adsp_native_selftest)
endif()

add_executable(kinst_vblank_fast_selftest tests/kinst_vblank_fast_selftest.cpp)
target_link_libraries(kinst_vblank_fast_selftest PRIVATE ki_runtime)
add_test(NAME kinst_vblank_fast_selftest COMMAND kinst_vblank_fast_selftest)

add_executable(mfc0_inline_selftest tests/mfc0_inline_selftest.cpp tests/empty_function_table.cpp)
target_compile_definitions(mfc0_inline_selftest PRIVATE KI_R36S_FAST_COP0=1)
target_link_libraries(mfc0_inline_selftest PRIVATE ki_runtime)
add_test(NAME mfc0_inline_selftest COMMAND mfc0_inline_selftest)

add_executable(mips_memory_fast_selftest tests/mips_memory_fast_selftest.cpp tests/empty_function_table.cpp)
target_compile_definitions(mips_memory_fast_selftest PRIVATE KI_R36S_FAST_MEMORY=1)
target_link_libraries(mips_memory_fast_selftest PRIVATE ki_runtime)
add_test(NAME mips_memory_fast_selftest COMMAND mips_memory_fast_selftest)

add_executable(gles2_rgba_upload_selftest tests/gles2_rgba_upload_selftest.cpp tests/empty_function_table.cpp)
target_link_libraries(gles2_rgba_upload_selftest PRIVATE ki_runtime)
add_test(NAME gles2_rgba_upload_selftest COMMAND gles2_rgba_upload_selftest)

if(KI_R36S)
  add_executable(dispatch_cache_selftest tests/dispatch_cache_selftest.cpp)
  target_link_libraries(dispatch_cache_selftest PRIVATE ki_runtime)
  add_test(NAME dispatch_cache_selftest COMMAND dispatch_cache_selftest)
  add_test(NAME dispatch_cache_baseline_selftest COMMAND dispatch_cache_selftest)
  set_tests_properties(dispatch_cache_baseline_selftest PROPERTIES
      ENVIRONMENT "KI_R36S_DISPATCH_BASELINE=1")
endif()

add_executable(mips_idle_skip_selftest tests/mips_idle_skip_selftest.cpp tests/empty_function_table.cpp)
target_link_libraries(mips_idle_skip_selftest PRIVATE ki_runtime)
add_test(NAME mips_idle_skip_selftest COMMAND mips_idle_skip_selftest)

if(KI_R36S AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(host_profiler_selftest tests/host_profiler_selftest.cpp)
  target_include_directories(host_profiler_selftest PRIVATE src)
  target_link_options(host_profiler_selftest PRIVATE -rdynamic)
  target_link_libraries(host_profiler_selftest PRIVATE ${CMAKE_DL_LIBS})
  add_test(NAME host_profiler_selftest COMMAND host_profiler_selftest)
  add_executable(gfx_profile_selftest tests/gfx_profile_selftest.cpp)
  target_compile_definitions(gfx_profile_selftest PRIVATE KI_R36S_GFX_PROFILE_SUPPORTED=1)
  add_test(NAME gfx_profile_selftest COMMAND gfx_profile_selftest)
  add_executable(audio_smooth_selftest tests/audio_smooth_selftest.cpp)
  add_test(NAME audio_smooth_selftest COMMAND audio_smooth_selftest)
  add_executable(boot_wipe_selftest tests/boot_wipe_selftest.cpp)
  target_include_directories(boot_wipe_selftest PRIVATE src)
  add_test(NAME boot_wipe_selftest COMMAND boot_wipe_selftest)
  # Building the graphics test suite must also materialize the newer
  # GLES2 RGB555/RGBA differential test before ctest --test-dir.
  add_dependencies(gfx_profile_selftest gles2_rgba_upload_selftest audio_smooth_selftest)
endif()
file(GLOB _cases LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/traces/poc/*")
foreach(case_dir IN LISTS _cases)
  if(IS_DIRECTORY "${case_dir}" AND EXISTS "${case_dir}/exit_regs.txt")
    get_filename_component(case_name "${case_dir}" NAME)
    add_test(NAME "poc_${case_name}" COMMAND compare_case "${case_dir}")
  endif()
endforeach()

# ---- whole program -------------------------------------------------------------
# Every entry point the translator can find (jal targets, code pointers in the
# data images) plus the entry and the exception handler, split into
# KI_SPLIT units so they compile in parallel. With cycle accounting and
# interrupt polling, for running on the board model.
option(KI_BUILD_FULL "build the whole-program executable ki_run" ON)
if(KI_BUILD_FULL)
  set(KI_SPLIT 16 CACHE STRING "number of generated translation units for the whole program")
  # Return sites that mid-game snapshots (tools/mame_snapshot.py) continue at
  # after their entry function returns: in the main loop after the per-frame
  # call at 0x880007e0, after the dispatcher call at 0x8800099c, and in the
  # main loop at 0x8802aa24 after its call to 0x88003e08.
  set(KI_EXTRA_ENTRIES "880007e8;880009a4;8802af78" CACHE STRING "additional entry points for the whole program")
  set(extra_args "")
  foreach(e IN LISTS KI_EXTRA_ENTRIES)
    list(APPEND extra_args --func ${e})
  endforeach()
  # Recompiled functions replaced by hand-written code in src/overrides/ (the
  # translation stays available as func_<addr>_original).
  set(KI_OVERRIDES "880107a8;8800e188;88018c98;88030b9c;88023008;8802309c;8803108c" CACHE STRING "functions provided by src/overrides")
  set(KI_OVERRIDE_SOURCES src/overrides/sprites.cpp src/overrides/fight.cpp)
  set(override_args "")
  foreach(o IN LISTS KI_OVERRIDES)
    list(APPEND override_args --override ${o})
  endforeach()
  set(FULL_DIR "${CMAKE_BINARY_DIR}/gen_full")
  set(full_srcs "${FULL_DIR}/recompiled_table.cpp")
  math(EXPR last "${KI_SPLIT} - 1")
  foreach(k RANGE ${last})
    list(APPEND full_srcs "${FULL_DIR}/recompiled_${k}.cpp")
  endforeach()
  set(data_args "")
  foreach(d IN LISTS KI_DATA_IMAGES)
    list(APPEND data_args --data ${d})
  endforeach()
  add_custom_command(
    OUTPUT ${full_srcs} "${FULL_DIR}/recompiled.h"
    COMMAND "${KI_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py"
            --image "${KI_CODE_IMAGE}" --vram 88000000 --expect-sha1 ${KI_CODE_IMAGE_SHA1} ${data_args}
            --func 88000000 --func 880266c8 ${extra_args} --discover --discover-pointers ${override_args}
            --out-dir "${FULL_DIR}" --split ${KI_SPLIT}
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py" "${KI_CODE_IMAGE}" ${data_deps}
    COMMENT "Recompiling the whole program"
    VERBATIM)
  add_library(ki_full_code STATIC ${full_srcs} ${KI_OVERRIDE_SOURCES})
  target_include_directories(ki_full_code PRIVATE "${FULL_DIR}")
  ki_code_library(ki_full_code)
  if(KI_R36S)
    target_compile_definitions(ki_full_code PRIVATE KI_R36S_FAST_COP0=1 KI_R36S_FAST_MEMORY=1)
    if(NOT KI_R36S_RELEASE_CLEAN)
      target_compile_definitions(ki_full_code PRIVATE KI_R36S_GFX_PROFILE_SUPPORTED=1)
    endif()
  endif()
  # Debug used -O1 even in Release, which leaves the recompiled guest
  # very slow on RK3326. Enable a conservative -O2 for handheld release builds.
  # Do NOT use -ffast-math (the guest needs accurate floating-point behavior)
  # or LTO until we have real hardware validation.
  if(NOT MSVC)
    if(KI_R36S AND CMAKE_BUILD_TYPE STREQUAL "Release")
      if(KI_R36S_MIPS_O3)
        # Test15 experimental second executable; -O3 may increase I-cache
        # pressure on Cortex-A35, so compare against the standard -O2 build.
        # No unsafe -ffast-math, timing, audio or semantic changes.
        target_compile_options(ki_full_code PRIVATE -O3 -Wno-unused-parameter)
      else()
        target_compile_options(ki_full_code PRIVATE -O2 -Wno-unused-parameter)
      endif()
    else()
      target_compile_options(ki_full_code PRIVATE -O1 -Wno-unused-parameter)
    endif()
  endif()
  # The boot EPROM's own code (reset vector, hardware init, the decompressor
  # that unpacks the game into DRAM), so the program can start from the
  # player's ROM instead of a MAME snapshot (ki_run/ki_play --boot rom).
  set(KI_BOOT_ROM_IMAGE "${CMAKE_SOURCE_DIR}/data/roms/kinst/ki-l15d.u98" CACHE FILEPATH
      "KI 1 boot EPROM U98, L1.5D (the code at its start runs at 0xbfc00000)")
  set(KI_BOOT_ROM_SHA1 "607394d4ba1713f38c2cb5159303cace9cde991e" CACHE STRING "expected SHA1 of KI_BOOT_ROM_IMAGE")
  set(BOOT_GEN "${CMAKE_BINARY_DIR}/gen_boot/boot_recompiled.cpp")
  add_custom_command(
    OUTPUT "${BOOT_GEN}"
    COMMAND "${KI_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py"
            --image "${KI_BOOT_ROM_IMAGE}" --vram bfc00000 --alias 9fc00000 --code-size 0xe48
            --expect-sha1 ${KI_BOOT_ROM_SHA1} --func bfc00000 --discover --jr-ra-jumps
            --table-symbol kBootFunctionTable --out "${BOOT_GEN}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py" "${KI_BOOT_ROM_IMAGE}"
    COMMENT "Recompiling the boot ROM"
    VERBATIM)
  add_library(ki_boot_rom_code STATIC "${BOOT_GEN}")
  ki_code_library(ki_boot_rom_code)
  if(KI_R36S)
    target_compile_definitions(ki_boot_rom_code PRIVATE KI_R36S_FAST_COP0=1 KI_R36S_FAST_MEMORY=1)
  endif()
  if(NOT MSVC)
    target_compile_options(ki_boot_rom_code PRIVATE -O1 -Wno-unused-parameter)
  endif()

  # compare_case against the whole program and the board model (for captures of
  # functions that call other code or touch I/O).
  add_executable(compare_full tests/compare_case.cpp)
  target_compile_definitions(compare_full PRIVATE KI_COMPARE_FULL)
  target_link_libraries(compare_full PRIVATE ki_full_code)

  # Same program generated with a per-instruction trace hook (lockstep debugging).
  option(KI_BUILD_TRACE "build ki_run_trace" OFF)
  if(KI_BUILD_TRACE)
    set(TRACE_DIR "${CMAKE_BINARY_DIR}/gen_trace")
    set(trace_srcs "${TRACE_DIR}/recompiled_table.cpp")
    foreach(k RANGE ${last})
      list(APPEND trace_srcs "${TRACE_DIR}/recompiled_${k}.cpp")
    endforeach()
    add_custom_command(
      OUTPUT ${trace_srcs} "${TRACE_DIR}/recompiled.h"
      COMMAND "${KI_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py"
              --image "${KI_CODE_IMAGE}" --vram 88000000 --expect-sha1 ${KI_CODE_IMAGE_SHA1} ${data_args}
              --func 88000000 --func 880266c8 ${extra_args} --discover --discover-pointers --trace ${override_args}
              --out-dir "${TRACE_DIR}" --split ${KI_SPLIT}
      DEPENDS "${CMAKE_SOURCE_DIR}/tools/kirecomp/kirecomp.py" "${KI_CODE_IMAGE}" ${data_deps}
      COMMENT "Recompiling the whole program with trace hooks"
      VERBATIM)
    add_library(ki_trace_code STATIC ${trace_srcs} ${KI_OVERRIDE_SOURCES})
    target_include_directories(ki_trace_code PRIVATE "${TRACE_DIR}")
    ki_code_library(ki_trace_code)
    if(NOT MSVC)
      target_compile_options(ki_trace_code PRIVATE -O1 -Wno-unused-parameter)
    endif()
    add_executable(ki_run_trace src/ki_run.cpp src/ki_boot.cpp src/ki_fight.cpp src/ki_sprites.cpp)
    target_link_libraries(ki_run_trace PRIVATE ki_boot_rom_code ki_trace_code)
  endif()

  # Reading the player's MAME set directly: kinst.chd through libchdr
  # (BSD-3-Clause), kinst.zip through the miniz it bundles (MIT). Fetched at a
  # fixed revision; not part of this repository.
  option(KI_WITH_CHD "read kinst.chd and kinst.zip directly (fetches libchdr)" ON)
  if(KI_WITH_CHD)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CHDR_WANT_TESTS OFF CACHE BOOL "" FORCE)
    set(MINIZ_ARCHIVE_APIS ON CACHE BOOL "" FORCE)
    set(MINIZ_STDIO ON CACHE BOOL "" FORCE)
    FetchContent_Declare(libchdr
      GIT_REPOSITORY https://github.com/rtissera/libchdr.git
      GIT_TAG 607694ca0812edfc9cc2030c64634fc2393668de)  # 2026-09-27
    FetchContent_MakeAvailable(libchdr)
    if(NOT MSVC)
      target_compile_options(chdr-static PRIVATE -w)
      target_compile_options(miniz PRIVATE -w)
    endif()
    add_library(ki_hd STATIC src/ki_hd.cpp src/ki_sprites.cpp)
    target_link_libraries(ki_hd PUBLIC ki_runtime miniz)
    target_include_directories(ki_hd PRIVATE "${libchdr_SOURCE_DIR}/deps/miniz-3.1.2")
    add_library(ki_files STATIC src/ki_files.cpp)
    target_link_libraries(ki_files PUBLIC chdr-static miniz)
    target_include_directories(ki_files PRIVATE "${libchdr_SOURCE_DIR}/include"
                               "${libchdr_SOURCE_DIR}/deps/miniz-3.1.2")
  endif()

  add_library(ki_boot STATIC src/ki_boot.cpp src/ki_fight.cpp)
  target_link_libraries(ki_boot PUBLIC ki_boot_rom_code ki_runtime)
  if(KI_WITH_CHD)
    add_executable(ki_run src/ki_run.cpp)
    target_link_libraries(ki_run PRIVATE ki_boot ki_hd ki_full_code)
    target_compile_definitions(ki_run PRIVATE KI_HAVE_HD)
  else()
    add_executable(ki_run src/ki_run.cpp src/ki_sprites.cpp)
    target_link_libraries(ki_run PRIVATE ki_boot ki_full_code)
  endif()

  # DCS model against a MAME capture (tools/mame_dcs_capture.py); not a ctest
  # because the capture is derived from the game and lives in traces/.
  add_executable(dcs_replay tests/dcs_replay.cpp)
  target_link_libraries(dcs_replay PRIVATE ki_boot ki_full_code)
  target_include_directories(dcs_replay PRIVATE src)

  # Windowed player: needs SDL2 (on macOS: brew install sdl2-compat or sdl2).
  find_package(SDL2 CONFIG QUIET)
  # Cross toolchains often provide pkg-config SDL2 instead of a CMake config.
  if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(KI_SDL2 QUIET IMPORTED_TARGET sdl2)
      if(TARGET PkgConfig::KI_SDL2)
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        target_link_libraries(SDL2::SDL2 INTERFACE PkgConfig::KI_SDL2)
        set(SDL2_FOUND TRUE)
      endif()
    endif()
  endif()
  if(SDL2_FOUND)
    add_executable(ki_play src/ki_play.cpp src/ki_pad.cpp src/ki_pad_ui.cpp)
    target_link_libraries(ki_play PRIVATE ki_boot ki_full_code SDL2::SDL2)
    if(KI_R36S)
      add_executable(r36s_intro_selftest tests/r36s_intro_selftest.cpp)
      target_include_directories(r36s_intro_selftest PRIVATE src)
      target_link_libraries(r36s_intro_selftest PRIVATE SDL2::SDL2)
      add_test(NAME r36s_intro_selftest COMMAND r36s_intro_selftest)
    endif()
    if(KI_R36S)
      target_compile_definitions(ki_play PRIVATE KI_R36S=1)
      if(KI_R36S_RELEASE_CLEAN)
        target_compile_definitions(ki_play PRIVATE KI_R36S_RELEASE_CLEAN=1)
      endif()
      if(KI_R36S_LEGACY_SDL_ABI)
        target_compile_definitions(ki_play PRIVATE KI_R36S_LEGACY_SDL_ABI=1)
      endif()
      if(KI_R36S_RELEASE_CLEAN)
        # All the per-frame profilers are omitted in the release player.
        # Do not export native generated symbols for the PC sampler.
        message(STATUS "R36S clean release enabled: no native PC sampling or timing probes")
      elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(ki_play PRIVATE -rdynamic)
        target_link_libraries(ki_play PRIVATE ${CMAKE_DL_LIBS})
      endif()
    endif()
    if(KI_WITH_CHD)
      target_link_libraries(ki_play PRIVATE ki_files ki_hd)
      target_compile_definitions(ki_play PRIVATE KI_HAVE_FILES KI_HAVE_HD)
    endif()
    if(KI_R36S AND NOT APPLE)
      # The guest runtime needs symbols from the generated function table, but
      # functions in the generated archive also refer back to the runtime.
      # GNU ld's single-pass static archive scan otherwise leaves the table
      # unresolved (undefined reference to ki::kFunctionTable[Size]).
      # LINK_GROUP:RESCAN emits --start-group/--end-group on GNU ld and keeps
      # link-order correctness regardless of dependent library ordering.
      if(CMAKE_VERSION VERSION_LESS "3.24")
        message(FATAL_ERROR "KI_R36S requires CMake >= 3.24 for LINK_GROUP:RESCAN")
      endif()
      target_link_libraries(ki_play PRIVATE "$<LINK_GROUP:RESCAN,ki_full_code,ki_runtime>")
    endif()
  else()
    if(KI_R36S)
      message(FATAL_ERROR "KI_R36S requires SDL2 development files for the target architecture")
    endif()
    message(STATUS "SDL2 not found: ki_play is not built")
  endif()

  # RK3326 has Cortex-A35 cores. Tune the native machine code without
  # changing emulated timing or applying unsafe floating point shortcuts.
  if(KI_R36S AND CMAKE_BUILD_TYPE STREQUAL "Release" AND
     CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|AARCH64")
    foreach(ki_r36s_target IN ITEMS ki_runtime ki_full_code ki_boot_rom_code
        ki_boot adsp2105 ki_play)
      target_compile_options(${ki_r36s_target} PRIVATE -mcpu=cortex-a35)
    endforeach()
    if(KI_WITH_CHD)
      target_compile_options(ki_files PRIVATE -mcpu=cortex-a35)
      target_compile_options(ki_hd PRIVATE -mcpu=cortex-a35)
      target_compile_options(chdr-static PRIVATE -mcpu=cortex-a35)
    endif()
    message(STATUS "R36S Cortex-A35 tuning active (no fast-math)")
  endif()
  # Test20 experimental second executable. IPO lets the native AArch64
  # compiler optimize/in-line across generated recompiled MIPS units and
  # emulator runtime libraries, but may affect cache pressure or behavior.
  # Keep the established Release/O3 code path as the default and compare
  # separately on RK3326 before enabling this for normal gameplay.
  if(KI_R36S AND KI_R36S_LTO)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
      message(FATAL_ERROR "KI_R36S_LTO requires -DCMAKE_BUILD_TYPE=Release")
    endif()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ki_ipo_ok OUTPUT _ki_ipo_error LANGUAGES CXX)
    if(NOT _ki_ipo_ok)
      message(FATAL_ERROR "R36S LTO not supported by this toolchain: ${_ki_ipo_error}")
    endif()
    foreach(_ki_target IN ITEMS ki_full_code ki_runtime ki_boot_rom_code
                                   ki_boot adsp2105 ki_play)
      set_property(TARGET ${_ki_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endforeach()
    if(KI_WITH_CHD)
      set_property(TARGET ki_files PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
      set_property(TARGET ki_hd PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    message(STATUS "R36S Test20 EXPERIMENTAL LTO enabled: do not ship as default without hardware A/B")
  endif()
endif()
