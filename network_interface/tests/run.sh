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
# Keep poll calls available to the linker wrapper instead of __poll_chk.
${CC:-cc} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -std=gnu11 -g -O1 -Wall -Wextra \
    ${SANITIZERS:--fsanitize=address,undefined} -fno-omit-frame-pointer \
    $LUA_CFLAGS -Iinclude -Inetwork_interface/include -I"$WSER_INCLUDE" \
    network_interface/tests/interface_test.c \
    network_interface/src/lua_start.c network_interface/src/worker_process.c \
    -Wl,--wrap=poll,--wrap=accept,--wrap=init_lua,--wrap=check_config_file \
    $LUA_LIBS -o "$build_dir/interface_test"
"$build_dir/interface_test"

# GCC: avoid retaining the unused Lua registration table through ASan globals.
${CC:-gcc} --param asan-globals=0 -std=gnu11 -g -O1 -ffunction-sections -fdata-sections \
    ${SANITIZERS:--fsanitize=address,undefined} -fno-omit-frame-pointer \
    $LUA_CFLAGS -Iinclude -Ilua/include \
    network_interface/tests/cache_eviction_test.c -Wl,--gc-sections \
    $LUA_LIBS -o "$build_dir/cache_eviction_test"
"$build_dir/cache_eviction_test"

${CC:-gcc} --param asan-globals=0 -std=gnu11 -g -O1 -ffunction-sections -fdata-sections \
    ${SANITIZERS:--fsanitize=address,undefined} -fno-omit-frame-pointer \
    $LUA_CFLAGS -Iinclude -Ilua/include \
    network_interface/tests/order_transaction_test.c -Wl,--gc-sections \
    $LUA_LIBS -o "$build_dir/order_transaction_test"
"$build_dir/order_transaction_test"

${CC:-cc} -std=gnu11 -g -O1 -ffunction-sections -fdata-sections \
    ${SANITIZERS:--fsanitize=address,undefined} -fno-omit-frame-pointer \
    -Iinclude network_interface/tests/file_io_test.c src/file.c \
    -Wl,--gc-sections,--wrap=read,--wrap=write -o "$build_dir/file_io_test"
"$build_dir/file_io_test"

${CC:-cc} -std=gnu11 -g -O1 -ffunction-sections -fdata-sections \
    ${SANITIZERS:--fsanitize=address,undefined} -fno-omit-frame-pointer \
    $LUA_CFLAGS -Iinclude -Inetwork_interface/include -I"$WSER_INCLUDE" \
    network_interface/tests/cache_maintenance_test.c -Wl,--gc-sections \
    $LUA_LIBS -o "$build_dir/cache_maintenance_test"
"$build_dir/cache_maintenance_test"
