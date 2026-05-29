# ts2fzy

`ts2fzy` is the TypeScript-first side of `j2fz`.

It lets JavaScript and TypeScript call native [Fzy](https://github.com/saint0x/fzy) libraries through the Fzy C ABI manifest and generated bindings.

## What It Does

- manifest parsing and ABI validation
- native library loading and symbol preflight
- typed binding generation
- callback registration helpers
- ownership-aware pointer handling
- generated package-local discovery loaders
- explicit raw runtime for low-level control

It also now contains the reverse-direction JavaScript host runtime used by `fzy2ts`:

- handle-based JS module loading
- explicit JS export/value/callback registrations
- explicit call and await lifecycle for Fzy-driven JS execution
- a native host shim that forwards the Fzy-facing C ABI into a JS host sidecar process

## When It Is Useful

- Node API services that need native hashing, parsing, validation, binary protocol handling, or storage helpers
- worker and pipeline systems doing ETL, content processing, binary transforms, or batch analysis
- infra CLIs and developer tools where TS owns UX and orchestration while Fzy owns heavy execution
- Electron or desktop apps where the UI is TS or React and Fzy provides local native capability
- local-first products that need fast indexing, search, sync helpers, or binary file handling
- fullstack apps where the frontend stays ordinary web tech and the backend uses Fzy for hot-path native modules

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

Run locally:

```sh
npm run check
npm test
npm run bench
npm run build:host-shim
```

## Docs

- [Architecture](docs/ARCHITECTURE.md)
- [Usage](docs/USAGE.md)
- [Ownership](docs/OWNERSHIP.md)
- [Callbacks And Async](docs/CALLBACKS_ASYNC.md)
- [Raw Unsafe API](docs/RAW_UNSAFE.md)
- [Benchmarking](docs/BENCHMARKING.md)
