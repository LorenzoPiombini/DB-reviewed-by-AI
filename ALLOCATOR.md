# Zone allocator integration

This patch applies after 7c6fa73. It retains the zone allocator and restores the
CRUD durability facade and exact file I/O from ea60fba/869acc8/4e39add.

## Ownership and lifetime

All DB records, strings, schemas, hash nodes, cache buffers and transaction
copies use the same zone in libcrud. Lua's C bindings use A_alloc/A_calloc/
A_strdup/A_free for these objects, including rollback copies. The zone is
single-threaded, matching the one sequential database worker. Initialization is
idempotent, explicit at Lua startup and lazy for standalone library callers.
Do not clear or close the zone between requests: cached pointers remain live.

A_Malloc returns zeroed storage. A_free clears the merged free region. Realloc
preserves the original allocation on failure, returns the same pointer when it
already fits, supports NULL and zero sizes, and preserves registered owners.
Owners must outlive their allocations. Database allocations are M_STATIC:
dirty caches must never be silently purged by the allocator. Cache maintenance
still flushes before eviction; transaction caches remain pinned.

The default zone is 8 MiB. Empty table buffers now start at 4 KiB and grow.
Allocation exhaustion returns NULL; there is no fallback heap allocation for
zone objects. Transactions need space for both originals and staged copies.
An 8 MiB zone is not a guarantee that every dataset fits, and it is NOT an 8 MiB
process RSS limit. Lua, transport buffers, the journal's bounded recovery
metadata, shared libraries, stacks and sanitizer overhead use additional memory.
If a durable commit fails (including allocation failure), the worker fails
closed and preserves the journal for recovery as before.

## Build and deploy

Stop the old WSER process tree and take an offline backup. Apply the patch in
DB-reviewed-by-AI. Rebuild all DB objects and reinstall the matching libraries,
Lua db module and libworker before restarting WSER. Rebuild consumers using the
updated headers as well. Do not mix old heap-owning modules with new zone-owning
modules. The allocator header is now installed with the other DB headers.

The Makefile keeps the Fedora compiler settings. allocator.o is part of libcrud;
the sibling DB shared libraries link to that single allocator implementation.
The library targets compile their object prerequisites; libraryPR also builds
and installs its matching libcrud. Avoid parallel debug/production builds into
the same shared-library output files. If using your existing installation
workflow, ensure it rebuilds the Lua module AFTER the new libcrud is available.

## Verification

With Lua development headers/libraries and the WSER include path available:

    WSER_INCLUDE=/path/to/WSER/include sh network_interface/tests/run.sh
    sh network_interface/tests/disk_roundtrip.sh
    WSER_INCLUDE=/path/to/WSER/include sh network_interface/tests/durable_crash.sh

Set LUA_CFLAGS and LUA_LIBS when Lua is not discoverable through pkg-config.
ASan/UBSan are enabled by default. In environments where LeakSanitizer cannot
inspect processes, ASAN_OPTIONS=detect_leaks=0 is required.

Coverage includes realloc/owner correctness, zeroing, alignment, exhaustion,
coalescing, partial schema cleanup under memory pressure, transaction allocation
failures, rollback with a full zone, fresh-process disk readback, SIGKILL during
commit and recovery, corrupt-journal rejection and injected write/fsync failures.
The zone is one libc allocation, so ASan alone cannot detect every write across
internal block boundaries. These tests are not proof that all legacy DB error
paths are correct, or a physical power-loss test.
