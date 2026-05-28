# j2fz

`j2fz` is a TypeScript-first bridge for calling native [Fzy](https://github.com/saint0x/fzy) libraries from JavaScript and TypeScript.

It treats the Fzy C ABI manifest as the source of truth and builds a typed JS runtime on top of it:

- JS calling Fzy exports
- Fzy calling JS callbacks
- explicit ownership and disposal
- async export support
- generated TypeScript bindings
- separate raw unsafe escape hatch for expert use

## When It Is Useful

`j2fz` is a good fit when TypeScript should stay the application surface and Fzy should own the native or performance-sensitive layer.

- Node API services that need native hashing, parsing, validation, binary protocol handling, or storage helpers
- worker and pipeline systems doing ETL, content processing, binary transforms, or batch analysis
- infra CLIs and developer tools where TS owns UX and orchestration while Fzy owns heavy execution
- Electron or desktop apps where the UI is TS or React and Fzy provides local native capability
- local-first products that need fast indexing, search, sync helpers, or binary file handling
- fullstack apps where the frontend stays ordinary web tech and the backend uses Fzy for hot-path native modules
- plugin systems that want typed JS bindings over ABI-stable native components

## What It Gives You

- manifest parsing and ABI validation
- native library loading and symbol preflight
- typed binding generation
- callback registration helpers
- ownership-aware pointer handling
- generated package-local discovery loaders
- explicit raw runtime for low-level control

## Performance

Current local benchmark coverage includes:

- module load
- scalar native calls
- borrowed buffer calls
- out-buffer writes
- callback roundtrips
- owned pointer allocate/decode/dispose
- raw unsafe scalar calls

Recent local numbers on Apple Silicon:

- safe scalar call: about `80 ns/op`
- raw scalar call: about `48 ns/op`
- safe borrowed buffer call: about `182 ns/op`
- safe out buffer call: about `101 ns/op`
- safe callback roundtrip: about `193 ns/op`
- safe owned pointer alloc/decode/dispose: about `1.33 us/op`

See [docs/BENCHMARKING.md](docs/BENCHMARKING.md) for the benchmark harness and measurement notes.

## Validation

The current runtime is covered by:

- strict TypeScript typecheck
- native integration tests against compiled fixture libraries
- async export integration tests
- callback lifecycle tests
- ownership misuse tests
- generated package consumer compile tests
- local performance benchmarks

Run locally:

```sh
npm run check
npm test
npm run bench
```

## Docs

- [Architecture](docs/ARCHITECTURE.md)
- [Usage](docs/USAGE.md)
- [Ownership](docs/OWNERSHIP.md)
- [Callbacks And Async](docs/CALLBACKS_ASYNC.md)
- [Raw Unsafe API](docs/RAW_UNSAFE.md)
- [Benchmarking](docs/BENCHMARKING.md)

## Status

The runtime and generator foundation are in a strong production state:

- typed safe runtime
- raw unsafe runtime
- async support
- callback support
- owned pointer lifecycle handling
- `repr(C)` struct roundtrip coverage
- generated package output
- realistic benchmark coverage
