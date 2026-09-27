# ERP production release gates

Status: not cleared for production. This hardening pass fixes specific defects;
it does not certify the server or database. The intended users are employees
of customer companies operating orders, customers, items, recipes and vendors.

## Changes in this patch

- File reads/writes now transfer the requested length, retry interrupted POSIX
  calls, and report EOF, zero progress and I/O errors. Previously short writes
  and truncated reads were silently accepted. Failure may leave a transferred
  prefix; this is not rollback or atomic I/O.
- Failed POSIX opens now return -1 and leave an invalid descriptor. Previously
  callers checking for -1 missed a positive errno return, and cleanup could
  mistake the errno value for an unrelated descriptor.
- Loading an empty data file reads zero bytes, regardless of its spare cache
  capacity. Failed/negative size queries fail before allocation.
- Cache maintenance runs independently of configuration-file availability.
  It flushes occupied caches every 20 minutes between requests and evicts
  caches unused for three hours, rather than comparing last-use time against
  creation time. Checks/retries for inactive caches are limited to once a
  minute. A failed flush retains that cache and does not skip other files.
  Clock rollback starts a new flush interval. This is periodic writeback,
  not a guarantee that an acknowledged operation is durable.
- Reload checks use the configuration path supplied to init_lua. Failed Lua
  loads no longer leave error objects accumulating on the stack. A runtime
  error can still partially change Lua globals: reload is not transactional.

The worker remains single and sequential. No additional worker, thread or
parallel cache access was introduced. Neither project's Makefile was changed.
Deploy rebuilt libcrud, the Lua module and libworker together.

## Validation

On Linux with GCC, Lua 5.4 and ASan/UBSan:

```sh
WSER_INCLUDE=/path/to/WSER/include \
LUA_CFLAGS="-DFEDORA $(pkg-config --cflags lua5.4)" \
LUA_LIBS="$(pkg-config --libs lua5.4)" \
sh network_interface/tests/run.sh

LUA_CFLAGS="-DFEDORA $(pkg-config --cflags lua5.4)" \
LUA_LIBS="$(pkg-config --libs lua5.4)" \
sh network_interface/tests/disk_roundtrip.sh
```

Override the Lua flags/library name for the installation. The runners build
in temporary directories; they do not install libraries or modify Makefiles.
The file-I/O tests inject partial transfers, EINTR, zero progress and ENOSPC
around real temporary files. Maintenance tests use a controlled clock and
substituted disk writes. Existing request, shutdown and transaction failure
tests continue to run.

The full-engine roundtrip uses real CRUD/index/schema code and the actual Lua
module. It creates two empty database files, commits 32 two-file transactions,
rolls back writes to both files on a Lua exception, flushes them and checks all
records from a fresh child process. There are no substituted persistence calls
in that test. It is a clean-flush test, not a power-loss recovery test.

LeakSanitizer was disabled for these runs because this execution environment
cannot perform its process inspection. Windows and the deployed Fedora binary
have not been validated by these Linux tests.

## Required before customer data

1. **Crash-safe durable commits.** write_cache_to_disk truncates the live index
   and data files separately; it does not fsync them or commit related files
   atomically. Order rollback protects in-memory failures only. SIGKILL cannot
   run a flush handler. Design a durable transaction log/checkpoint protocol
   covering every write operation and all related files, with recovery before
   serving requests. Verify kill/power-loss simulations and ENOSPC/fsync errors
   at every commit boundary. Never acknowledge durable success before the
   recovery record is durably committed. Merely adding fsync or independent
   renames does not make a multi-file order atomic.
2. **Employee identity and permissions.** WSER's current request/DB dispatch
   has no demonstrated login/session enforcement. Each request needs an
   authenticated employee, company membership and a permission for its action.
   An anonymous request must fail before reaching the worker. Orders, shipping,
   customer maintenance, recipes and vendor maintenance need explicit roles.
3. **Company isolation.** Decide between one deployment/database per company
   and a shared deployment. A shared deployment needs tenant-scoped keys,
   indexes, reads, writes, reports, cache identity and backups. Never trust a
   browser-supplied company ID or filename as authorization. Test cross-company
   reads/writes and ID enumeration. The current fixed file configuration does
   not implement this isolation for a shared multi-company ERP.
4. **Deployment lifecycle.** Enforce exactly one writer over the database,
   including during rapid restarts and CLI maintenance. Existing Unix socket
   setup can unlink an old socket, so a pathname alone is not a singleton lock.
   Run with a dedicated account and private DB/socket directories; remove the
   hardcoded /root assumptions before deploying that way. Verify service stop,
   restart, DB flush failures, port release and recovery on the actual host.
5. **Previously reported crash.** The Fedora ASan null-write trace attributed
   to worker_process/poll has not been explained. Local passing tests do not
   resolve that trace. Capture the faulting instruction/backtrace with the
   exact deployed binary and matching libraries before release.
6. **Recovery and operations.** Demonstrate restoring a consistent backup on
   another host. Until a consistent online snapshot exists, copy DB files only
   after all writers have stopped and a successful flush is confirmed. Test
   corrupt/truncated files, bounded memory, concurrent employee workloads,
   slow clients, disk exhaustion and prolonged operation. Collect and alert on
   worker exits, writeback errors and failed recovery. Sanitizer builds belong
   in testing; all production libraries and the executable need a consistent
   reviewed release build and a rollback procedure.

Do not infer production readiness from a successful HTTP response, clean
shutdown persistence, or a short RSS plateau alone.
