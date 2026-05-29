# Callbacks And Async

## Callback Model

Callbacks are contract-driven and must be declared in the Fozzy ABI manifest.

`j2fz` currently supports:

- callback registration by binding id
- typed generated callback helpers
- typed callback handle returns
- typed callback context aliases for context-bearing bindings
- explicit callback disposal

Runtime guarantees:

- missing callback binding ids are rejected
- non-function callback registration values are rejected
- callback registration emits diagnostics when enabled
- callback invocation, completion, and failure emit diagnostics when enabled
- disposed callback handles are removed from the loaded module’s active callback set
- callback handle disposal is idempotent
- use after module disposal is rejected explicitly

Generated bindings surface callback registrations as methods like:

```ts
bindings.register_with_callback_main((value, ctx) => value + 1);
```

## Async Model

Async exports follow the Fozzy `async-handle-v1` contract.

`j2fz` exposes them as `Promise`-returning methods in generated bindings.

Runtime behavior:

- bind async boundary symbols at module load time
- call `start`
- poll until completion
- call `await`
- drop the handle on success or failure
- time out if the handle does not complete within `asyncTimeoutMs`

Diagnostics currently cover:

- `async.started`
- `async.completed`
- `async.poll_failed`
- `async.await_failed`
- `async.failed`
- `async.timed_out`

The async helper state machine also has direct unit coverage separate from the native fixture integration tests.
The native integration suite also covers:

- borrowed async input adaptation from JS strings into native byte buffers
- async owned-pointer return adaptation into disposable JS handles
