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
- Bound recursive table decoding, reserve Lua stack space, reject trailing data,
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

1. **Forced termination can lose acknowledged writes.** This is a write-back
   database. `close_lua()` does not flush caches, and SIGKILL cannot perform
   cleanup. The prior WSER process-tree termination patch is therefore not a
   database durability guarantee. A coordinated shutdown needs to stop accepting
   new work, complete in-flight operations, flush/check storage in normal process
   context, acknowledge completion, and only then exit the process tree.
2. **Flush is not crash-atomic.** `src/crud.c:write_cache_to_disk` writes the
   index, truncates the live data file, then writes cached data. An interruption
   can leave data/index state inconsistent. Schema persistence is still marked
   TODO there. A transaction/recovery design and crash-injection tests are
   required before claiming safe restarts or power-loss durability.
3. **Cache maintenance timing is wrong.** `free_inactive_caches` computes
   `used - now_seconds()` for its idle timeout, normally negative. It also runs
   only around requests/accept, and missing configuration bypasses maintenance.
   Correcting this must be paired with the storage flush review above.
4. **Order writes can partially succeed.** `write_orders` validates and writes
   each line in the same loop, then writes the header. A later invalid line or
   write failure leaves earlier writes behind; there is no rollback. It also
   references `SALES_ORDER_LINE_WRITE_FAILED`, whereas the declared constant is
   plural. Its `nil, error_code` return is truncated by the current `t>l` bridge
   call, losing the specific error. Whole-order validation and transactional
   writes need a separate tested change.

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
