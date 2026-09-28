# Durable commits for the Linux WSER database worker

## What is covered

The worker now commits successful customer, item and new-order requests before
sending their success replies. A single commit includes every loaded cache's
index, record data and schema. An order's header and lines, and a customer's
secondary index and record, therefore belong to the same recovery unit.

This is a whole-cache redo journal, not a compact operation-level WAL. It keeps
the existing database file formats and the single sequential worker. It adds no
threads or processes and changes neither Makefile. The journal implementation
is included from src/crud.c so the existing libcrud build exports its functions.

## Commit and recovery protocol

1. Acquire an exclusive lifetime flock in DB_DIRECTORY/.wser-durable/lock.
   Recover pending work before initializing Lua or loading any records.
2. Execute one request in RAM. Eviction and direct-disk write fallbacks cannot
   publish partial changes while a mutating request is active.
3. Serialize occupied caches into a private building directory. Write complete
   index/data/schema snapshots, fsync every file, and record their absolute
   destinations, lengths and CRC32 checksums in a versioned manifest. Fsync the
   manifest, its length/checksum seal and the snapshot directory.
4. Rename building to pending and fsync the journal parent directory. This
   publishes one redo set containing all related files. Before publication,
   live files are untouched. A crash before publication discards building.
5. Validate the entire pending redo set before applying anything. Stream each
   snapshot to a temporary file next to its destination, fsync it, rename it
   over the destination and fsync that destination's parent directory. Keep all
   journal snapshots intact throughout this process so replay is repeatable.
6. After every destination is durable, rename pending to done, fsync the journal
   directory, remove the retired snapshots and sync the directory again. Only
   then send success. Startup repeats steps 5–6 if pending is present.

No other request executes during these steps. A kill during file replacement
can leave mixed live files temporarily, but the worker does not serve them:
startup must complete recovery before Lua loads data. A damaged/incomplete
committed journal fails startup and is retained for investigation. Checksums
are corruption detection, not protection against an administrator modifying
files. The storage system must honor fsync and atomic rename semantics.

A failed application request discards its loaded caches, including secondary
index changes, and reloads the last committed state on subsequent reads. Any
commit/storage failure stops the worker without a further shutdown flush; its
outcome is uncertain until recovery. It never sends a successful response for
that failed commit. A crash after commit but before delivery of the reply can
still leave a saved order with no confirmation in the browser. This is not
exactly-once delivery; check the existing record before retrying. Request
idempotency keys are separate future work.

## Deployment

This patch builds on the previously supplied production-hardening patch
(upstream 4e39add). It is Linux-only; unsupported systems refuse durable startup.

1. Stop the old WSER process tree cleanly and confirm all workers have exited.
   Take a consistent offline backup of the database first. Do not replace
   libraries while an old worker is still running.
2. Apply this patch in DB-reviewed-by-AI and rebuild/reinstall libcrud, the Lua
   db module, libworker, and the updated worker headers together using your
   existing build workflow. Ensure the dynamic linker loads the matching new
   copies; mixed old/new libraries are unsupported. The DB Makefile adds
   obj/durable.o to libcrud; other build settings are unchanged.
   durable.c is compiled separately, with the worker API exposed through crud.h.
3. The default database directory is /root/db, matching the current Lua config.
   To use another directory, set WSER_DB_DIRECTORY in the worker's environment
   and adjust db_config.lua's table paths to that same database tree. The root
   must already exist, belong to the worker's effective UID and not be group-
   or world-writable. The journal directory is created with mode 0700 and must
   remain private and owned by that UID. Tables must already exist as ordinary
   files within that tree; hard-linked destination files are refused.
4. Start WSER normally. An existing pending journal is recovered automatically;
   a second durable worker on the same directory is refused. Check startup
   errors and complete a write/restart/read test on a staging copy of the actual
   company's data before deploying it to the live droplet.

Do not delete .wser-durable/pending to get past a recovery error. Retain the
whole database tree, free disk space or fix the filesystem problem, then retry
startup with the matching software. Do not run an old binary against a database
with a pending journal. Downgrade only after this version completes recovery
and all writers stop. The old binary does not understand the journal protocol.

All data access must go through the one durable worker while it is running.
The legacy CLI/raw CRUD API does not participate in this protocol or its lock;
concurrent CLI access, multiple differently configured roots over the same data,
external writers and online directory copies are unsupported. Take backups
while the worker is stopped after a successful checkpoint. Restore with the
same absolute table paths if a retained journal needs replay.

Configuration hot-reload is disabled in durable-worker mode because executing
Lua configuration could bypass the request transaction boundary. Restart to
change it. TEST cannot bypass caching in this mode. Legacy direct-disk Lua
create_record and delete_record operations are rejected; the current worker
already rejects UPDATE_SORD and exposes no delete endpoint. They need a cached,
journaled implementation before being enabled as future ERP operations.

## Cost and limits

Each write snapshots every occupied cache, then checkpoints those snapshots.
The additional temporary disk space is roughly the sum of snapshot sizes plus
one largest destination file, along with metadata. Snapshot/replay copies use
bounded buffers, but existing cache and order-rollback RAM costs remain.
Expect more I/O and higher write latency as the database grows. Measure it on
real customer-sized data; large commits can exceed the existing HTTP/IPC timeout
and produce an uncertain client result even when the commit is durable.

The implementation protects against process crashes and is designed for power
loss when the filesystem/device honors synchronization. Tests here simulate
process death and I/O errors, not a physical power cut of a DigitalOcean host.
It does not fix the previously reported Fedora ASan crash, add backups, or
provide employee authentication/authorization.

## Validation

From the DB repository, set LUA_CFLAGS/LUA_LIBS for your Lua 5.4 installation
and WSER_INCLUDE to the WSER include directory, then run:

    sh network_interface/tests/run.sh
    sh network_interface/tests/durable_crash.sh
    sh network_interface/tests/disk_roundtrip.sh

The new suite links the real CRUD/index/schema engine, Lua module and worker
storage initialization. It exercises:

- SIGKILL at 29 commit boundaries, including snapshot creation, publication,
  each file replacement, completed checkpoint and journal retirement.
- Repeated SIGKILL during recovery, followed by successful full replay.
- A second process failing to acquire the database writer lock.
- Corrupt snapshot, manifest and seal rejection before live-file replacement.
- ENOSPC at write-call positions and EIO at fsync-call positions; after restart
  the transaction is entirely old or entirely new, never partially applied.
- Real customer creation, secondary indexes, successful durable commit,
  failed-request cache discard and fresh-process readback.
- Worker replies observed before/after a substituted commit: no early success,
  and a commit failure stops the worker with an error response.

These and the existing interface, shutdown, cache, transaction and clean-disk
roundtrip regressions passed with ASan/UBSan. LeakSanitizer is disabled in this
execution environment. src/crud.c also compiles with the existing Fedora C89,
-Werror and sanitizer flags. This is not a production readiness certification;
validate the actual deployed build, filesystem and workloads as well.
