# WSER database interface review

Base: `8ffc749` (`main`). This pass reviews the active root Makefile's
`libworker.so` path: `worker_process.c` -> `lua_start.c` -> Lua database
functions. Neither Makefile nor the stored database files/binaries are changed.
The older `load_db.c` text protocol is outside this patch.

## Fixed in this patch

- Validate packet bounds before reading the write header or subtracting its
  length; reject oversized SOCK_SEQPACKET records instead of parsing a prefix.
- Initialize replies on every request, preventing undefined reads and stale
  response data on malformed writes.
- Bound report signature copies and accept only `>s`, matching the C arguments
  actually supplied. Other signatures could previously overrun the buffer or
  make the variadic Lua bridge access nonexistent arguments.
- Bound recursive table decoding, reserve Lua stack space, reject
  invalid array tags, embedded NUL field names, and incomplete/nonfinite numeric
  values. Restore the Lua stack after failed calls and between requests.
- Handle interrupted accept/receive/send calls, close failed key-list sockets,
  suppress socket SIGPIPE, and apply five-second receive/send timeouts.
- Return listener failure to WSER instead of sending SIGINT to `getppid()`.
  That PID can change after reparenting, including becoming PID 1; a library
  worker must not use it as an implicit shutdown target.
- Format 64-bit IDs correctly, JSON-escape item names, reject numeric lookup
  keys above UINT32_MAX, and preserve complete small key-list JSON replies.
- Check write status codes, explicitly reject unimplemented order updates,
  detect report acknowledgement EOF, and remove an accidental 16-bit report
  length limit. Reports still use a single SOCK_SEQPACKET message, so the
  operating system's maximum packet size remains a limit.
- Close Lua state after initialization failures; make close safe to repeat.

`get_function_signature` now takes the destination capacity as its third
argument. Rebuild libworker and recompile any external callers of that function.
The wire operation numbers and response framing remain unchanged. Network
hardening here targets the Linux Unix-domain SOCK_SEQPACKET transport used by
WSER; this is not a TCP framing implementation.

## Remaining findings requiring storage/integration work

1. **SIGKILL still cannot flush.** The shutdown extension below handles
   SIGTERM/SIGINT and Linux parent death, but direct SIGKILL or power loss can
   still discard cached writes. A supervisor must allow the database worker
   time to finish, and must not immediately follow SIGTERM with SIGKILL.
2. **Flush is not crash-atomic.** `src/crud.c:write_cache_to_disk` writes the
   index, truncates the live data file, then writes cached data. An interruption
   can leave data/index state inconsistent. Schema persistence is still marked
   TODO there. A transaction/recovery design and crash-injection tests are
   required before claiming safe restarts or power-loss durability.
3. **Cache maintenance timing is wrong.** `free_inactive_caches` computes
   `used - now_seconds()` for its idle timeout, normally negative. It also runs
   only around requests/accept, and missing configuration bypasses maintenance.
   Correcting this must be paired with the storage flush review above.
4. **Order rollback now covers operation failures in memory.** The extension
   below stages the head and line caches together. Failed writes and Lua errors
   discard both staged versions. Later multi-file disk flushing is still not
   crash-atomic and needs recovery/journaling to survive an interrupted flush.

These findings are based on source inspection. This patch does not establish
storage durability or validate the database engine as a whole.

## Regression tests

On Linux with a C compiler and Lua 5.4 development package:

```sh
WSER_INCLUDE=/path/to/WSER/include network_interface/tests/run.sh
```

The runner uses pkg-config for Lua, with `LUA_CFLAGS` and `LUA_LIBS` overrides
for custom installations. It does not use or change either Makefile, install
anything, or access live database files.

Tests compile the actual worker and Lua bridge and use real Lua 5.4 and Unix
SOCK_SEQPACKET socket pairs. Only accept, configuration initialization/checks,
and storage/cache functions are substituted. Tests exercise malformed tokens,
truncation, nesting limits, signature bounds, stack recovery, oversized/short
packets, disconnected clients, report acknowledgement EOF, interrupted accept,
100 consecutive key-list requests, socket closure, write errors, JSON escaping,
64-bit response IDs, numeric lookup bounds, and a 70,000-byte report.

Passed with AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer could
not run in the review environment because it cannot inspect process tasks;
`ASAN_OPTIONS=detect_leaks=0` was required there. Lua itself was an uninstrumented
system runtime. No full deployed WSER/database, real persistence, power-loss,
or macOS tests were performed.

## Shutdown extension (after interface patch 3216cc5)

`work_process` installs SIGTERM/SIGINT handlers before initializing Lua. The
handler only sets a `sig_atomic_t` flag. The normal worker loop stops accepting
new work, lets an already executing Lua call return, invokes
`flush_lua_caches()`, then closes Lua. Every occupied slot is written through the
existing `write_cache_to_disk()` routine, including slots whose `used` timestamp
is zero. A failed flush is logged by file name; remaining slots are still
attempted. The worker returns -1 on any flush failure, or 0 after a successful
signal-requested shutdown. The owning WSER process must propagate failure;
returning an error does not recover a failed disk write.

The listener is temporarily nonblocking and polled at 250 ms intervals to avoid
missing a termination request immediately before a blocking accept. Accepted
client I/O retains its five-second timeouts. A nonreturning Lua function or
blocked storage operation can still delay shutdown. Existing handlers and
listener flags are restored on return.

On Linux the database worker changes its inherited parent-death signal from
SIGKILL to SIGTERM before Lua initialization and checks for parent disappearance
during setup. This permits flushing when WSER's HTTP/TLS parent exits. Parent
death before this setup may still invoke the previously inherited SIGKILL, but
this worker has not yet initialized its Lua cache at that point. A restarted
server must not start a second database writer against the same files until the
old database worker has actually finished flushing.

Use `kill -TERM <database-worker-pid>` (or ordinary `kill <pid>`). Ctrl-C sends
SIGINT, which follows the same path. `kill -9` / `pkill -9` sends SIGKILL and
cannot run cleanup. This extension makes no changes to either Makefile.

Additional tests use real signal delivery during idle poll/client receive, a
termination request during a Lua call, repeated signals during flushing, first
and last cache slots, and an injected flush failure. A forked worker also
inherits SIGKILL as its parent-death signal; its parent exits after worker
initialization, and the test verifies two flush calls and successful return.
Storage functions are substituted to verify ordering and error handling; the
actual disk writer is not exercised by these shutdown tests. ASan/UBSan pass.

This extension calls the existing disk writer unchanged: it does not add fsync,
transactional file replacement, missing schema persistence, or rollback.
Those storage limitations listed above remain. A successful shutdown flush is
not a power-loss durability guarantee.

## Parser buffer-bound correction

The table decoder's `data_size` is a readable-buffer bound, not a requirement
that one table consume every byte. The extra `bwalked != data_size` rejection
introduced in 3216cc5 has been removed. Per-read bounds checks still reject
truncated tables. Regression tests accept a complete table followed by zero or
nonzero unused bytes and check that the decoded value is unchanged.

## Cache eviction and order-write follow-up

- Full-cache eviction now removes the old filename from `cache_register` and
  frees that register node before freeing/reusing the slot. Previously a lookup
  for the evicted file could return the reused slot and access another file's
  cached contents. Failure to flush or remove the register entry retains the
  slot and reports failure.
- `write_orders` validates the head/line table shape, positive integer line
  count, matching array length, and every finite positive quantity before key
  generation or any line write. Previously an invalid later line left earlier
  lines written. A failed key allocation is now reported before writes.
- The active order writer now uses the declared plural line-error constant.
  Successful calls return `key, 0`; errors retain `nil, error_code`. The C bridge
  requests both results so it can preserve the error code instead of discarding
  it. Deploy the updated worker library and Lua configuration together.

Regression tests run the real Lua configuration with substituted storage calls
and test the actual eviction function with substituted cache/register operations.
They verify no writes for malformed orders (including an invalid second line),
line/head/key failure codes, successful ordering, removing stale registrations,
and preserving cache contents on eviction failures. ASan/UBSan pass with leak
checking disabled in this environment as documented above. The eviction test
uses GCC's `--param asan-globals=0` to let the linker discard unused Lua entry
points; other compiler choices need an equivalent setting. These are not
on-disk crash-recovery tests. Both Makefiles are unchanged.

## Order rollback extension

`network_interface/lua/db_config.lua:write_orders` validates the request, then
runs key generation, all line appends, and the header append inside
`db.order_transaction(head_file, lines_file, callback)`. The callback returns
`key, 0` on success or `nil, error_code` on failure. The wrapper is implemented
in the existing Lua module source, so neither Makefile changes.

Before invoking the callback the module loads both files into cache and clones
all their index chains and cached record bytes. Schema and filename storage
remain shared and immutable for this append-only operation. The worker runs
against the staged buffers; the cache register keeps the same two slot numbers.
On success the originals are released. On any reported write failure, malformed
callback result or protected Lua exception, staged buffers are discarded and
both original caches restored, without allocation or I/O during rollback.
Snapshot allocation or cache-load failure aborts before the callback runs.

Transactions cannot nest. Cache eviction, direct disk test mode, unrelated
files, and update/delete/create/index-edit APIs are excluded while an order
transaction is active. The normal cached append routine handles the writes;
there is no direct-disk fallback for these pinned files. Existing sequential
single-worker execution makes the two-cache swap unobservable between requests.
SIGTERM/SIGINT handlers only set a flag, so shutdown flushing occurs after the
protected order call has committed or rolled back.

Also corrected the order's write-result checks: the module returns `nil,
"error message"` on failure. Checking only the second return value used to
mistake that error string for a successful record. Both the key and record must
now be non-nil. Transaction setup/Lua errors use code -25 and receive an error
response from the worker. Deploy db.so, libworker.so and db_config.lua together;
missing transaction support fails closed before order writes.

Tests run the real transaction implementation and real Lua order function with
substituted record writes and file loading. They modify record bytes and both
primary/secondary index chains before injected failures at the first line,
second line and header; each must restore the original buffers and indexes.
Coverage includes Lua exceptions, successful retry after rollback, each of 16
snapshot allocation failures, unavailable files, nested transactions, unrelated
file access, invalid callback results, and disabled disk test mode. Existing
interface, eviction and shutdown suites also pass ASan/UBSan (LeakSanitizer is
unavailable here).

Costs/limits: staging temporarily duplicates the two files' cache buffers and
indexes, so large orders/files need additional RAM. This is logical rollback
for order creation, not a general transaction API or write-ahead log. It does
not make later cache flushing atomic or durable against SIGKILL/power loss.
No real disk crash-recovery test is claimed.
