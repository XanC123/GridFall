# Gridfall's Raylib dependency patch.
# Gameplay uses a hidden absolute pointer + recenter fallback on WSLg/XWayland,
# so it intentionally does not patch or depend on SDL relative-motion values.
# This file retains the strict-warning fix for Raylib's bundled jar_mod reader.

if (NOT DEFINED raylib_SOURCE_DIR)
    message(FATAL_ERROR "Gridfall expected raylib_SOURCE_DIR to be defined")
endif()

# Raylib's bundled jar_mod reader ignores fread's result, which triggers
# -Wunused-result on toolchains that annotate stdio reads. Check the read and
# avoid attempting to parse a partial buffer.
set(_gridfall_jar_mod_path "${raylib_SOURCE_DIR}/src/external/jar_mod.h")
if (NOT EXISTS "${_gridfall_jar_mod_path}")
    message(FATAL_ERROR "Gridfall expected Raylib's bundled src/external/jar_mod.h")
endif()
file(READ "${_gridfall_jar_mod_path}" _gridfall_jar_mod_source)
set(_gridfall_fread_before [=[            fread(modctx->modfile, fsize, 1, f);]=])
set(_gridfall_fread_after [=[            size_t bytesRead = fread(modctx->modfile, fsize, 1, f);
            if (bytesRead != 1)
            {
                JARMOD_FREE(modctx->modfile);
                modctx->modfile = 0;
                modctx->modfilesize = 0;
                fsize = 0;
            }]=])
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_fread_before}" _gridfall_fread_old_index)
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_fread_after}" _gridfall_fread_new_index)
set(_gridfall_jar_mod_changed FALSE)
if (_gridfall_fread_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_fread_before}" "${_gridfall_fread_after}" _gridfall_jar_mod_source "${_gridfall_jar_mod_source}")
    set(_gridfall_jar_mod_changed TRUE)
elseif (_gridfall_fread_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib jar_mod fread call to check.")
endif()
set(_gridfall_jar_mod_load_before [=[            if(!jar_mod_load(modctx, (void *)modctx->modfile, fsize)) fsize = 0;]=])
set(_gridfall_jar_mod_load_after [=[            if (fsize != 0 && !jar_mod_load(modctx, (void *)modctx->modfile, fsize)) fsize = 0;]=])
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_jar_mod_load_before}" _gridfall_jar_load_old_index)
string(FIND "${_gridfall_jar_mod_source}" "${_gridfall_jar_mod_load_after}" _gridfall_jar_load_new_index)
if (_gridfall_jar_load_old_index GREATER_EQUAL 0)
    string(REPLACE "${_gridfall_jar_mod_load_before}" "${_gridfall_jar_mod_load_after}" _gridfall_jar_mod_source "${_gridfall_jar_mod_source}")
    set(_gridfall_jar_mod_changed TRUE)
elseif (_gridfall_jar_load_new_index LESS 0)
    message(FATAL_ERROR "Could not locate Raylib jar_mod load call to guard after a failed read.")
endif()
if (_gridfall_jar_mod_changed)
    file(WRITE "${_gridfall_jar_mod_path}" "${_gridfall_jar_mod_source}")
    message(STATUS "Gridfall checked Raylib jar_mod fread result")
endif()
