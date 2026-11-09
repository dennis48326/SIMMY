cmake_minimum_required(VERSION 3.25)
project(simmy LANGUAGES C CXX)

# SIMMY: PC port of The Sims 2 (GameCube), built on aurora.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Deterministic floats (lockstep netplay: Windows and Linux builds must compute the same simulation,
# docs/determinism-and-netplay.md): IEEE single/double on SSE2, no fused multiply-add contraction (a
# build for an FMA-capable -march would otherwise fuse a*b+c differently per compiler), no fast-math.
# For everything, aurora included (its PSVEC/PSMTX math feeds game state). The game's sin/cos/atan2/...
# are src/platform/detmath (the GameCube's libm), never the C library's.
if (MSVC)
  add_compile_options(/fp:precise)  # MSVC 2022: no contraction unless /fp:contract; never /fp:fast
else ()
  add_compile_options(-ffp-contract=off -fno-fast-math)
endif ()

set(SIMMY_AURORA_DIR "" CACHE PATH "Local aurora checkout (leave empty to fetch it with git)")
set(SIMMY_AURORA_TAG "505b5ebc91d585948f41ff5b6ec14694af078891" CACHE STRING "aurora git ref to fetch when SIMMY_AURORA_DIR is empty")

include(FetchContent)
# The port's aurora patches (cmake/aurora_patches) go into the aurora tree before it is added.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SimmyAuroraPatches.cmake)
option(SIMMY_AURORA_PATCH "Apply cmake/aurora_patches to the aurora tree (also to SIMMY_AURORA_DIR)" ON)
if (SIMMY_AURORA_DIR)
  if (SIMMY_AURORA_PATCH)
    simmy_patch_aurora(${SIMMY_AURORA_DIR})
  endif ()
  add_subdirectory(${SIMMY_AURORA_DIR} aurora EXCLUDE_FROM_ALL)
else ()
  # SOURCE_SUBDIR names a folder that does not exist, so MakeAvailable only downloads aurora; it is
  # patched, then added.
  FetchContent_Declare(aurora
    GIT_REPOSITORY https://github.com/encounter/aurora.git
    GIT_TAG ${SIMMY_AURORA_TAG}
    SOURCE_SUBDIR simmy-populate-only
    )
  FetchContent_MakeAvailable(aurora)
  if (SIMMY_AURORA_PATCH)
    simmy_patch_aurora(${aurora_SOURCE_DIR})
  endif ()
  add_subdirectory(${aurora_SOURCE_DIR} ${aurora_BINARY_DIR})
endif ()

# --- generated layer: class/method declarations + stubs for everything not yet ported -------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SIMMY_GEN ${CMAKE_CURRENT_SOURCE_DIR}/gen)
set(SIMMY_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_port.py)

file(GLOB_RECURSE SIMMY_PORTED CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)
list(FILTER SIMMY_PORTED EXCLUDE REGEX "/src/launcher/")  # SimmyLauncher, its own target (below)

# Run once at configure time so the stub file list exists...
execute_process(
  COMMAND ${Python3_EXECUTABLE} ${SIMMY_GENERATOR} ${CMAKE_CURRENT_SOURCE_DIR}
  RESULT_VARIABLE _gen_result)
if (NOT _gen_result EQUAL 0)
  message(FATAL_ERROR "gen_port.py failed")
endif ()
file(GLOB SIMMY_STUBS ${SIMMY_GEN}/src/stubs/*.cpp)

# ...and again whenever ported code or signature overrides change (it only rewrites changed files).
add_custom_target(simmy_gen
  COMMAND ${Python3_EXECUTABLE} ${SIMMY_GENERATOR} ${CMAKE_CURRENT_SOURCE_DIR}
  BYPRODUCTS ${SIMMY_GEN}/include/simmy/types_gen.hpp ${SIMMY_STUBS}
  DEPENDS ${SIMMY_PORTED} ${CMAKE_CURRENT_SOURCE_DIR}/overrides.tsv ${SIMMY_GENERATOR}
  COMMENT "Regenerating declarations and stubs")

add_executable(simmy ${SIMMY_PORTED} ${SIMMY_STUBS})
add_dependencies(simmy simmy_gen)
target_include_directories(simmy PRIVATE include ${SIMMY_GEN}/include)
target_precompile_headers(simmy PRIVATE ${SIMMY_GEN}/include/simmy/types_gen.hpp)
target_link_libraries(simmy PRIVATE aurora::core aurora::gx aurora::main aurora::vi aurora::os aurora::pad aurora::card aurora::mtx aurora::thp)
if (MSVC)
  target_compile_options(simmy PRIVATE /W3 /bigobj /wd4100 /wd4068)
else ()
  target_compile_options(simmy PRIVATE -Wall -Wno-unused-parameter -Wno-unknown-pragmas)
endif ()

# PC graphics settings: put the commented default simmy.ini next to the executable once (an existing
# one, possibly edited, is left alone).
add_custom_command(TARGET simmy POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/simmy.ini.example -DDST=$<TARGET_FILE_DIR:simmy>/simmy.ini
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/copy_if_missing.cmake
  VERBATIM)

# Windows: copy every runtime DLL simmy links against (Dawn, SDL3, fmt, ...) plus the DLLs Dawn loads
# itself (dxcompiler, dxil) next to simmy.exe, so it starts from the build folder.
if (WIN32)
  if (SIMMY_AURORA_DIR)
    set(SIMMY_AURORA_SRC ${SIMMY_AURORA_DIR})
  else ()
    set(SIMMY_AURORA_SRC ${aurora_SOURCE_DIR})
  endif ()
  include(${SIMMY_AURORA_SRC}/cmake/AuroraCopyRuntimeDLLs.cmake)
  aurora_copy_runtime_dlls(simmy)
endif ()

# SimmyLauncher (src/launcher): the game's launcher, SimmyLauncher.exe next to simmy.exe. SDL3 only
# (aurora's copy); its fonts are compiled in.
set(SIMMY_LAUNCHER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/launcher)
set(SIMMY_LAUNCHER_GEN ${CMAKE_CURRENT_BINARY_DIR}/launcher_gen)
set(SIMMY_LAUNCHER_FONTS)
foreach (font bold medium)
  if (font STREQUAL "bold")
    set(_ttf ${SIMMY_LAUNCHER_DIR}/fonts/Poppins-Bold.ttf)
  else ()
    set(_ttf ${SIMMY_LAUNCHER_DIR}/fonts/Poppins-Medium.ttf)
  endif ()
  add_custom_command(OUTPUT ${SIMMY_LAUNCHER_GEN}/launcher_font_${font}.inc
    COMMAND ${CMAKE_COMMAND} -DSRC=${_ttf} -DDST=${SIMMY_LAUNCHER_GEN}/launcher_font_${font}.inc
            -DNAME=launcher_font_${font} -P ${SIMMY_LAUNCHER_DIR}/embed_file.cmake
    DEPENDS ${_ttf} ${SIMMY_LAUNCHER_DIR}/embed_file.cmake
    COMMENT "Embedding launcher font ${font}" VERBATIM)
  list(APPEND SIMMY_LAUNCHER_FONTS ${SIMMY_LAUNCHER_GEN}/launcher_font_${font}.inc)
endforeach ()
file(GLOB SIMMY_LAUNCHER_SOURCES CONFIGURE_DEPENDS ${SIMMY_LAUNCHER_DIR}/*.cpp ${SIMMY_LAUNCHER_DIR}/*.hpp)
add_executable(simmy_launcher WIN32 ${SIMMY_LAUNCHER_SOURCES} ${SIMMY_LAUNCHER_FONTS})
if (WIN32)
  target_sources(simmy_launcher PRIVATE ${SIMMY_LAUNCHER_DIR}/res/simmy_launcher.rc)
endif ()
set_target_properties(simmy_launcher PROPERTIES OUTPUT_NAME SimmyLauncher)
# (include/: simmy/version.hpp, the port's version)
target_include_directories(simmy_launcher PRIVATE ${SIMMY_LAUNCHER_DIR} ${SIMMY_LAUNCHER_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(simmy_launcher PRIVATE ${AURORA_SDL3_TARGET})
if (MSVC)
  target_compile_options(simmy_launcher PRIVATE /W4 /utf-8)
  target_compile_definitions(simmy_launcher PRIVATE _CRT_SECURE_NO_WARNINGS)
else ()
  target_compile_options(simmy_launcher PRIVATE -Wall -Wextra)
endif ()
if (WIN32)
  aurora_copy_runtime_dlls(simmy_launcher)
endif ()

# Startup probes (Windows diagnostics): tiny programs linking one dependency each.
if (WIN32)
  add_executable(probe_plain EXCLUDE_FROM_ALL tools/probes/probe_plain.cpp)
  add_executable(probe_sdl EXCLUDE_FROM_ALL tools/probes/probe_sdl.cpp)
  target_link_libraries(probe_sdl PRIVATE ${AURORA_SDL3_TARGET})
  add_executable(probe_dawn EXCLUDE_FROM_ALL tools/probes/probe_dawn.cpp)
  target_link_libraries(probe_dawn PRIVATE dawn::webgpu_dawn)
  foreach (probe probe_sdl probe_dawn)
    aurora_copy_runtime_dlls(${probe})
  endforeach ()
endif ()
