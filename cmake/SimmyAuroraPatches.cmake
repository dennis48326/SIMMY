# SIMMY: small changes the port needs in aurora, applied to the aurora source tree at configure time.
#
# Each cmake/aurora_patches/*.patch (a `git diff` against SIMMY_AURORA_TAG) is applied once, in name
# order: a patch that is already in the tree (it reverses cleanly) is skipped, so re-configuring, a
# fresh FetchContent download and an existing build folder all work. A patch that neither applies nor
# is already applied stops the configure with a message (another aurora revision, or local edits).
#
#   0001-two-window-present.patch  co-op "windows" display mode (pc_splitscreen.cpp, pc_window2.cpp):
#     AuroraSetRenderSizeOverride (GXAurora.h): a fixed EFB size, so one EFB can hold both players' views
#     at their windows' resolution; aurora::gfx::set_present_crop (gfx.hpp): present a rectangle of the
#     EFB, stretched, to a present target (the main window and the second window each get their half).
#     Without a crop set, aurora behaves exactly as before.
#   0002-larger-frame-buffers.patch  the per-frame vertex and index staging (lib/gfx/resources.hpp)
#     raised from 5 / 2 MiB to 12 / 4 MiB: with the PC mod's extra players (src/game/mp4.cpp) a frame
#     draws the lot four times, which overran the 5 MiB vertex buffer (aurora aborts on overflow).
#   0003-card-base-dir.patch  aurora_card_set_base_dir (card.h): before CARDInit, the folder the memory
#     cards go under instead of the user folder (PC mod: network play, src/platform/pc_net.cpp: the joining
#     PC plays on a copy of the host's card in its session folder, never on its own card).
#   0004-texture-stat-totals.patch  aurora::gx::texture_stats_total (lib/gx/texture.hpp): the texture
#     cache's per-frame counters (conversions + uploads, evictions, hits) summed over all frames, for the
#     frame-hitch profiler (src/platform/pc_perf.cpp; texture_stats() is reset at every frame's end).
#   0005-frame-buffer-overflow.patch  larger per-frame buffers (vertices 32, indices 8, vertex arrays
#     48 MiB: a lot full of converted PC objects overflowed the 8 MiB vertex-array buffer, which aborted),
#     and a full buffer no longer aborts: the rest of that frame's draws are dropped and it is logged;
#     aurora_frame_buffer_capacity (gfx.h) for the port's "[gfx] frame buffers" high-water log.

function(simmy_patch_aurora source_dir)
  file(GLOB _patches ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/aurora_patches/*.patch)
  list(SORT _patches)
  if (NOT _patches)
    return()
  endif ()
  find_package(Git QUIET)
  if (NOT GIT_FOUND)
    message(FATAL_ERROR "SIMMY: git is needed to apply the aurora patches in cmake/aurora_patches")
  endif ()
  # The patches applied are listed in <aurora>/.simmy_patches (name and SHA1 of each): a later patch may
  # change lines an earlier one changed (0005 the buffer sizes of 0002), after which the earlier one no
  # longer reverses cleanly, so "reverses cleanly" is only asked of patches the list does not name (a tree
  # patched by an older SIMMY build, before the list existed).
  set(_stamp ${source_dir}/.simmy_patches)
  set(_done "")
  if (EXISTS ${_stamp})
    file(STRINGS ${_stamp} _done)
  endif ()
  foreach (_patch IN LISTS _patches)
    get_filename_component(_name ${_patch} NAME)
    file(SHA1 ${_patch} _sha)
    if ("${_name} ${_sha}" IN_LIST _done)
      message(STATUS "SIMMY: aurora patch ${_name} is applied")
      continue()
    endif ()
    # Already applied (by a build before the list)?
    execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check --ignore-whitespace ${_patch}
                    WORKING_DIRECTORY ${source_dir} RESULT_VARIABLE _applied OUTPUT_QUIET ERROR_QUIET)
    if (_applied EQUAL 0)
      message(STATUS "SIMMY: aurora patch ${_name} is applied")
    else ()
      execute_process(COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace ${_patch}
                      WORKING_DIRECTORY ${source_dir} RESULT_VARIABLE _result ERROR_VARIABLE _error)
      if (NOT _result EQUAL 0)
        message(FATAL_ERROR "SIMMY: cannot apply ${_patch} to ${source_dir}:\n${_error}\n"
                            "The aurora tree must be SIMMY_AURORA_TAG (${SIMMY_AURORA_TAG}) without local changes "
                            "to the patched files (delete the build folder's _deps\\aurora-src and "
                            "_deps\\aurora-subbuild to download it again).")
      endif ()
      message(STATUS "SIMMY: applied aurora patch ${_name}")
    endif ()
    list(APPEND _done "${_name} ${_sha}")
    list(JOIN _done "\n" _text)
    file(WRITE ${_stamp} "${_text}\n")
  endforeach ()
endfunction()
