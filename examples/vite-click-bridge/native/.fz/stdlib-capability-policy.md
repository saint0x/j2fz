# Stdlib Capability Policy

- Schema: `fozzylang.stdlib_capability_policy.v1`
- Capability propagation: `explicit_compiler_checked`
- Token delegation: `compiler_enforced_subset_only`
- JSON boundary rule: `json_at_boundaries` / `typed_structs_and_enums`

## Module Contracts

| Module | Capability | Ownership | Errors | Linear Handles | Cleanup | Thread/Async Safety |
| --- | --- | --- | --- | --- | --- | --- |
| `core.mem` | `mem` | owned values + explicit alloc/free lifecycle | runtime status + verifier ownership diagnostics | heap pointers and owned allocations are linear | cleanup with `free(...)` or `defer free(...)` | thread-safe only through owned handoff; not raw-borrow safe across async/task boundaries |
| `core.http` | `http` | request/response bodies and network handles are owned | parse vs timeout vs transport errors remain distinct | HttpHandle/HttpStreamHandle/WebSocketHandle are linear | cleanup with `close(...)`, `http.stream_close(...)`, or `http.websocket_close(...)` | not send-safe; async-stable for owned handles only |
| `core.proc` | `proc` | argv/env builders and process handles are owned | runtime status + last-error boundary | ProcessArgv/ProcessEnv/ProcessHandle are linear | cleanup with `proc.close(...)`; builders must be consumed by spawn/run | process handles are async-stable but not send-safe; builders are neither |
| `core.fs` | `fs` | file handles are owned; path arguments are borrowed | runtime status + host error mapping | FileHandle is linear | cleanup with `fs.close(...)`; durable writes prefer `fs.atomic_write(...)` | file handles are async-stable but not send-safe |
| `core.thread` | `thread` | task handles and task groups are owned linear resources | task result / cancellation / timeout policy is explicit | TaskHandle and TaskGroupHandle are linear | terminate with `join`, `detach`, `cancel_task`, `task.group_join_all`, or `task.group_cancel` | send-safe task handles/groups only; borrowed values may not cross task boundaries |
| `core.time` | `time` | time values are plain owned data | status-free deterministic time/runtime APIs | no linear handles | no explicit cleanup required | thread-safe and async-safe |
| `core.crypto` | `rng` | crypto outputs are owned plain values | decode and runtime-status failures are explicit | no linear handles | no explicit cleanup required | thread-safe and async-safe; secret comparisons should use constant-time helpers |
| `core.json` | `http|fs|proc boundary payloads` | JSON stays at boundaries; typed structs/enums stay inside | parse failures stay explicit; raw injection is policy-checked | JsonHandle/ListHandle/MapHandle are owned non-linear handles | no explicit cleanup; avoid `json.raw(...)` except for primitive/raw boundary escapes | send-safe and async-stable owned collection handles |
| `core.log` | `log` | log payload maps/strings are owned values | runtime status for sink/config failures | no linear handles | no explicit cleanup required | thread-safe and async-safe logging facade |

## Strict Hazards

- `json_raw_composite_or_dynamic_injection` (`warning`): prefer `json.object`, `json.array`, or `json.str` over `json.raw(...)` for composite or user-shaped payloads
- `path_traversal_literal` (`warning`): literal filesystem paths containing `..` are rejected as traversal-prone in strict mode
- `shell_process_builder` (`warning`): shell command construction through `sh`/`bash` and `-c` is flagged in strict mode; prefer direct argv builders
- `tempfile_non_atomic_write` (`warning`): writing directly into `/tmp` or `/var/tmp` is flagged when a durable atomic write is expected
- `http_header_non_normalized` (`warning`): HTTP headers should be lowercase normalized tokens in strict mode
- `crypto_secret_eq` (`warning`): secret-bearing comparisons should use `crypto.constant_time_eq` or `security.secure_eq`
