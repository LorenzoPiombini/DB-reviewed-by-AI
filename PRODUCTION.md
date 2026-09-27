# ERP production release gates

Status: not cleared for production. See DURABILITY.md for the new worker journal. This hardening pass fixes specific defects;
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
  not by itself a guarantee that an acknowledged operation is durable. The newer
  durable-worker commit path in DURABILITY.md now supplies that guarantee for
  supported worker writes, subject to its filesystem and deployment assumptions.
- Reload checks use the configuration path supplied to init_lua. Failed Lua
  loads no longer leave error objects accumulating on the stack. A runtime
  error can still partially change Lua globals: reload is not transactional.
  The newer durable worker disables hot-reload; restart for configuration changes.

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

1. **Crash-safe durable commits.** Implemented for the Linux worker's currently
   supported write endpoints in DURABILITY.md: whole-cache redo snapshots,
   fsynced publication/checkpoints, recovery before reads and acknowledgements
   only after commit. Process-kill/replay and injected I/O error tests pass.
   Staging verification on the deployed Fedora build/storage, realistic write
   latency, and physical power-loss behavior remain deployment release gates.
   Legacy CLI/direct-disk operations are outside this guarantee.
2. **Employee identity and permissions.** WSER's current request/DB dispatch
   has no demonstrated login/session enforcement. Each request needs an
   authenticated employee, company membership and a permission for its action.
   An anonymous request must fail before reaching the worker. Orders, shipping,
   customer maintenance, recipes and vendor maintenance need explicit roles.
3. **Company isolation.** The chosen model is one droplet and one database per
   customer company. Keep deployment credentials, employee access and backups
   separate for each company. This does not replace employee permissions within
   an ERP instance. Shared multi-company database hosting is outside this design.
4. **Deployment lifecycle.** Durable workers now hold an exclusive database-root
   lock. Legacy CLI tools do not participate; keep them offline while the worker
   runs. Wait for the old process tree to exit before restarting, since WSER's
   Unix socket setup can unlink an existing socket. Run with a dedicated account
   and private DB/socket directories; remove the hardcoded /root configuration
   assumptions before deploying that way. Verify stop, restart, port release and
   recovery with the actual deployed service and libraries.
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
