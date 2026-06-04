# Release Policy

- Schema: `fozzylang.release_policy.v1`
- Compatibility set required: `true`
- Benchmark artifact: `artifacts/bench_corelibs_rust_vs_fzy.json`
- Stability dashboard command: `fz stability-dashboard`

## Compatibility

- `diagnosticCatalogVersion`: `fozzylang.diagnostic_catalog.v1`
- `languageVersion`: `fozzylang.language.v1`
- `manifestSchemaVersion`: `fozzy.run_manifest.v1`
- `nativeImportTableVersion`: `fozzylang.native_runtime_contracts.v1`
- `runtimeAbiVersion`: `fozzylang.runtime.v0`
- `traceSchemaVersion`: `fozzy-trace.v4`

## Error Model

- Service functions: `Result<T, Error>`
- CLI main: `i32`
- HTTP handlers: `i32_after_writing_response`
- Runtime internals: `typed_status_or_result`

- `transport`: boundary and IO failures at runtime or service edges (codes: Io)
- `parse`: invalid input and decode failures (codes: InvalidInput)
- `timeout`: deadline and wait exhaustion (codes: Timeout)
- `policy`: capability, conflict, and safety-policy violations (codes: Conflict)
- `internal`: not-found and internal runtime/compiler failure states (codes: NotFound, Internal)

## Benchmark Lanes

- `cli_startup`: CLI startup latency
- `http_throughput`: HTTP request throughput
- `json_build_parse`: JSON construction and parsing
- `proc_spawn_wait`: process spawn and wait
- `stream_reading`: stream reading throughput
- `task_group_execution`: task-group execution
- `compiler_parse_lower_build`: compiler parse, lower, and build time
- `native_binary_size`: native binary size

## Implementation-Backed Docs

- `language-policy`: .fz/language-policy.json + .fz/language-policy.md (`compiler syntax-freeze and profile metadata`)
- `native-runtime-contracts`: .fz/native-runtime-contracts.json + .fz/native-runtime-contracts.md (`native runtime contract table`)
- `release-policy`: .fz/release-policy.json + .fz/release-policy.md (`compiler release-policy metadata`)
- `diagnostic-catalog`: fz explain catalog --json (`diagnostic catalog metadata`)
- `stability-dashboard`: artifacts/stability_dashboard.json (`exit criteria and perf-source metadata`)
