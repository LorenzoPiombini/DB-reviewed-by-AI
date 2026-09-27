#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
: "${WSER_INCLUDE:?Set WSER_INCLUDE to the WSER include directory containing json.h}"
if [ -z "${LUA_CFLAGS:-}" ]; then
    LUA_CFLAGS="-DFEDORA $(pkg-config --cflags lua5.4)"
fi
if [ -z "${LUA_LIBS:-}" ]; then
    LUA_LIBS=$(pkg-config --libs lua5.4)
fi
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
# Flag variables deliberately support multiple compiler arguments.
${CC:-cc} -std=gnu11 -g -O1 -Wall -Wextra \
    ${SANITIZERS:--fsanitize=address,undefined} -fno-omit-frame-pointer \
    $LUA_CFLAGS -Iinclude -Inetwork_interface/include -I"$WSER_INCLUDE" \
    network_interface/tests/interface_test.c \
    network_interface/src/lua_start.c network_interface/src/worker_process.c \
    -Wl,--wrap=accept,--wrap=init_lua,--wrap=check_config_file \
    $LUA_LIBS -o "$build_dir/interface_test"
"$build_dir/interface_test"
