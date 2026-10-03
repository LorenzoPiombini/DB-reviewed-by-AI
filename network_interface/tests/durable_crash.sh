#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
if [ -z "${LUA_CFLAGS:-}" ]; then
    LUA_CFLAGS="-DFEDORA $(pkg-config --cflags lua5.4)"
fi
if [ -z "${LUA_LIBS:-}" ]; then LUA_LIBS=$(pkg-config --libs lua5.4); fi
: "${WSER_INCLUDE:?Set WSER_INCLUDE to the WSER include directory}"
config_path="$(pwd)/network_interface/lua/db_config.lua"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
# Match the existing Makefile: date.c uses strict-C89 libc time fields.
${CC:-cc} -std=c89 -g -O1 -fno-omit-frame-pointer \
    ${SANITIZERS:--fsanitize=address,undefined} -Iinclude \
    -c src/date.c -o "$build_dir/date.o"
# The actual storage engine and Lua module; no mocked persistence functions.
${CC:-cc} -std=gnu11 -g -O1 -fno-omit-frame-pointer \
    ${SANITIZERS:--fsanitize=address,undefined} \
    -DDB_DURABILITY_TEST_HOOK -Wl,--wrap=write,--wrap=fsync,--wrap=luaL_newstate \
    $LUA_CFLAGS -Iinclude -Ilua/include -Inetwork_interface/include -I"$WSER_INCLUDE" \
    network_interface/tests/durable_crash_test.c lua/src/export_db_lua.c network_interface/src/lua_start.c \
    src/crud.c src/allocator.c src/durable.c src/file.c "$build_dir/date.o" src/hash_tbl.c src/debug.c src/common.c \
    src/string_utilities.c src/str_op.c src/lock.c src/record.c src/endian.c \
    src/parse.c src/globals.c src/sort.c src/input.c src/key.c \
    $LUA_LIBS -lm -o "$build_dir/durable_crash_test"
"$build_dir/durable_crash_test" "$config_path"
