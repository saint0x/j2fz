# Benchmarking

`j2fz` includes a local benchmark harness for measuring core runtime overhead.

Run:

```sh
npm run bench
```

The harness focuses on repeatable local measurements of:

- module load time
- scalar sync call throughput
- borrowed-buffer call throughput
- larger borrowed-buffer throughput
- callback roundtrip throughput
- owned-pointer allocate/decode/dispose throughput
- raw symbol call throughput

## Benchmarking Rules

- compile the native fixture before timed sections
- warm up each call path before measuring
- take multiple timed samples
- report median and average timing, not a single best run
- keep benchmark logic separate from correctness tests

## Interpreting Results

These benchmarks are useful for:

- regressions between local changes
- comparing safe and raw paths
- understanding rough overhead classes

They are not a substitute for:

- full production workload profiling
- cross-machine absolute comparisons
- latency claims without hardware and environment context
