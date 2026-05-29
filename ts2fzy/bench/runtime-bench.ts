import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { loadFozzyModule } from "../src/runtime/loader.js";
import { loadUnsafeRawModule } from "../src/runtime/raw.js";
import type { FozzyAbiManifest } from "../src/types/abi.js";

interface FixturePaths {
  readonly libraryPath: string;
  readonly manifestPath: string;
}

interface MeasurementSummary {
  readonly label: string;
  readonly iterationsPerSample: number;
  readonly sampleCount: number;
  readonly totalIterations: number;
  readonly medianNsPerOp: number;
  readonly meanNsPerOp: number;
  readonly minNsPerOp: number;
  readonly maxNsPerOp: number;
}

function sharedLibraryName(stem: string): string {
  switch (process.platform) {
    case "darwin":
      return `lib${stem}.dylib`;
    case "win32":
      return `${stem}.dll`;
    default:
      return `lib${stem}.so`;
  }
}

function compileBenchmarkFixture(root: string): FixturePaths {
  const sourcePath = join(root, "bench_fixture.c");
  const libraryPath = join(root, sharedLibraryName("bench_fixture"));
  const manifestPath = join(root, "bench_fixture.abi.json");

  writeFileSync(
    sourcePath,
    `
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int32_t add_one(int32_t value) {
  return value + 1;
}

uint32_t hash32(const uint8_t *ptr, size_t len) {
  uint32_t acc = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    acc ^= ptr[i];
    acc *= 16777619u;
  }
  return acc;
}

int32_t invoke_i32_callback(int32_t value, int32_t (*cb)(int32_t, void *), void *cb_ctx) {
  if (cb == NULL) return -1;
  return cb(value, cb_ctx);
}

int32_t fill_bytes(uint8_t *buf_out, size_t len) {
  for (size_t i = 0; i < len; i++) {
    buf_out[i] = (uint8_t)(i + 1);
  }
  return (int32_t)len;
}

uint8_t *alloc_bytes(size_t len) {
  uint8_t *buf = (uint8_t *)malloc(len + 1);
  if (buf == NULL) return NULL;
  for (size_t i = 0; i < len; i++) {
    buf[i] = (uint8_t)('A' + (int)i);
  }
  buf[len] = 0;
  return buf;
}

void alloc_bytes_free(uint8_t *ptr) {
  free(ptr);
}
`,
    "utf8",
  );

  const manifest: FozzyAbiManifest = {
    schemaVersion: "fozzylang.ffi_abi.v1",
    package: {
      name: "bench.fixture",
      version: "0.0.1",
    },
    abiRevision: 1,
    targetTriple: process.arch,
    dataLayoutHash: "bench-layout",
    compilerIdentityHash: "bench-cc",
    panicBoundary: "error",
    layoutPolicy: {
      reprCStableOnly: true,
      nonReprCUnstable: true,
    },
    symbolVersioning: "strict-name-signature-v1",
    contractSchema: "fozzylang.ffi_contracts.v1",
    reprCLayouts: [],
    exports: [
      {
        name: "add_one",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "value",
            fzy: "i32",
            c: "int32_t",
            contract: {
              ownership: "value",
              nullability: "n/a",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
        ],
        return: {
          fzy: "i32",
          c: "int32_t",
          contract: {
            ownership: "value",
            nullability: "n/a",
            mutability: "const",
          },
        },
        contract: {
          execution: "sync",
          callbackBindings: [],
          asyncBoundary: null,
        },
      },
      {
        name: "hash32",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "ptr_borrowed",
            fzy: "*u8",
            c: "const uint8_t*",
            contract: {
              ownership: "borrowed",
              nullability: "non_null",
              mutability: "const",
              lifetimeAnchor: "loan:ptr",
              view: {
                kind: "ptr_len",
                lengthParam: "len",
              },
            },
          },
          {
            name: "len",
            fzy: "usize",
            c: "size_t",
            contract: {
              ownership: "value",
              nullability: "n/a",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
        ],
        return: {
          fzy: "u32",
          c: "uint32_t",
          contract: {
            ownership: "value",
            nullability: "n/a",
            mutability: "const",
          },
        },
        contract: {
          execution: "sync",
          callbackBindings: [],
          asyncBoundary: null,
        },
      },
      {
        name: "invoke_i32_callback",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "value",
            fzy: "i32",
            c: "int32_t",
            contract: {
              ownership: "value",
              nullability: "n/a",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
          {
            name: "cb_callback",
            fzy: "*u8",
            c: "void*",
            contract: {
              ownership: "borrowed",
              nullability: "non_null",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
          {
            name: "cb_ctx",
            fzy: "*u8",
            c: "void*",
            contract: {
              ownership: "borrowed",
              nullability: "nullable",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
        ],
        return: {
          fzy: "i32",
          c: "int32_t",
          contract: {
            ownership: "value",
            nullability: "n/a",
            mutability: "const",
          },
        },
        contract: {
          execution: "sync",
          callbackBindings: [
            {
              callbackParam: "cb_callback",
              contextParam: "cb_ctx",
              bindingId: "cbctx_invoke_i32",
              obligation: "context_outlives_callback_registration",
              lifetime: "registered",
              signature: {
                returnCType: "int32_t",
                params: [
                  { name: "value", c: "int32_t" },
                  { name: "cb_ctx", c: "void*" },
                ],
              },
            },
          ],
          asyncBoundary: null,
        },
      },
      {
        name: "fill_bytes",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "buf_out",
            fzy: "*u8",
            c: "uint8_t*",
            contract: {
              ownership: "out",
              nullability: "non_null",
              mutability: "mut",
              lifetimeAnchor: "loan:buf",
              view: {
                kind: "ptr_len",
                lengthParam: "len",
              },
            },
          },
          {
            name: "len",
            fzy: "usize",
            c: "size_t",
            contract: {
              ownership: "value",
              nullability: "n/a",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
        ],
        return: {
          fzy: "i32",
          c: "int32_t",
          contract: {
            ownership: "value",
            nullability: "n/a",
            mutability: "const",
          },
        },
        contract: {
          execution: "sync",
          callbackBindings: [],
          asyncBoundary: null,
        },
      },
      {
        name: "alloc_bytes",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "len",
            fzy: "usize",
            c: "size_t",
            contract: {
              ownership: "value",
              nullability: "n/a",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
        ],
        return: {
          fzy: "*u8",
          c: "uint8_t*",
          contract: {
            ownership: "owned",
            nullability: "non_null",
            mutability: "mut",
          },
        },
        contract: {
          execution: "sync",
          callbackBindings: [],
          asyncBoundary: null,
        },
      },
    ],
  };
  writeFileSync(manifestPath, JSON.stringify(manifest, null, 2), "utf8");

  const compiler = process.env.CC ?? "cc";
  const args =
    process.platform === "darwin"
      ? ["-dynamiclib", "-O3", "-o", libraryPath, sourcePath]
      : process.platform === "win32"
        ? ["/LD", sourcePath, `/Fe:${libraryPath}`]
        : ["-shared", "-fPIC", "-O3", "-o", libraryPath, sourcePath];
  const result = spawnSync(compiler, args, { encoding: "utf8" });
  if (result.status !== 0) {
    throw new Error(`benchmark fixture compile failed: ${result.stderr || result.stdout}`);
  }

  return { libraryPath, manifestPath };
}

function maybeRunGc(): void {
  if (typeof global.gc === "function") {
    global.gc();
  }
}

function runTimedSamples(
  label: string,
  iterationsPerSample: number,
  sampleCount: number,
  fn: () => void,
): MeasurementSummary {
  const samples: number[] = [];
  for (let sampleIndex = 0; sampleIndex < sampleCount; sampleIndex += 1) {
    maybeRunGc();
    const startedAt = process.hrtime.bigint();
    for (let iteration = 0; iteration < iterationsPerSample; iteration += 1) {
      fn();
    }
    const elapsedNs = Number(process.hrtime.bigint() - startedAt);
    samples.push(elapsedNs / iterationsPerSample);
  }

  const sorted = [...samples].sort((left, right) => left - right);
  const medianNsPerOp = sorted[Math.floor(sorted.length / 2)] ?? 0;
  const total = samples.reduce((sum, value) => sum + value, 0);
  return {
    label,
    iterationsPerSample,
    sampleCount,
    totalIterations: iterationsPerSample * sampleCount,
    medianNsPerOp,
    meanNsPerOp: total / sampleCount,
    minNsPerOp: sorted[0] ?? 0,
    maxNsPerOp: sorted[sorted.length - 1] ?? 0,
  };
}

function warmup(iterations: number, fn: () => void): void {
  for (let iteration = 0; iteration < iterations; iteration += 1) {
    fn();
  }
}

function formatNsPerOp(value: number): string {
  if (value >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(3)} ms/op`;
  }
  if (value >= 1_000) {
    return `${(value / 1_000).toFixed(3)} us/op`;
  }
  return `${value.toFixed(1)} ns/op`;
}

function formatSummary(summary: MeasurementSummary): string {
  return [
    `${summary.label}:`,
    `  median ${formatNsPerOp(summary.medianNsPerOp)}`,
    `  mean ${formatNsPerOp(summary.meanNsPerOp)}`,
    `  min ${formatNsPerOp(summary.minNsPerOp)}`,
    `  max ${formatNsPerOp(summary.maxNsPerOp)}`,
    `  samples ${summary.sampleCount} x ${summary.iterationsPerSample} iterations`,
  ].join("\n");
}

function main(): void {
  const root = mkdtempSync(join(tmpdir(), "j2fz-bench-"));
  try {
    const fixture = compileBenchmarkFixture(root);
    const payload = Buffer.from("benchmark-payload-0123456789", "utf8");
    const largePayload = Buffer.alloc(4096, 7);
    const outBuffer = Buffer.alloc(64);

    warmup(3, () => {
      const module = loadFozzyModule({
        paths: {
          sharedLibrary: fixture.libraryPath,
          abiManifest: fixture.manifestPath,
        },
        package: {
          name: "bench.fixture",
          version: "0.0.1",
        },
        ownedPointerReleasers: {
          alloc_bytes: "alloc_bytes_free",
        },
      });
      module.dispose();
    });
    const moduleLoad = runTimedSamples("safe module load", 5, 8, () => {
      const module = loadFozzyModule({
        paths: {
          sharedLibrary: fixture.libraryPath,
          abiManifest: fixture.manifestPath,
        },
        package: {
          name: "bench.fixture",
          version: "0.0.1",
        },
        ownedPointerReleasers: {
          alloc_bytes: "alloc_bytes_free",
        },
      });
      module.dispose();
    });

    const safeModule = loadFozzyModule({
      paths: {
        sharedLibrary: fixture.libraryPath,
        abiManifest: fixture.manifestPath,
      },
      package: {
        name: "bench.fixture",
        version: "0.0.1",
      },
      ownedPointerReleasers: {
        alloc_bytes: "alloc_bytes_free",
      },
    });
    const rawModule = loadUnsafeRawModule({
      paths: {
        sharedLibrary: fixture.libraryPath,
        abiManifest: fixture.manifestPath,
      },
      package: {
        name: "bench.fixture",
        version: "0.0.1",
      },
    });

    try {
      const safeAddOne = safeModule.exports.get("add_one");
      const safeHash32 = safeModule.exports.get("hash32");
      const safeInvokeCallback = safeModule.exports.get("invoke_i32_callback");
      const safeFillBytes = safeModule.exports.get("fill_bytes");
      const safeAllocBytes = safeModule.exports.get("alloc_bytes");

      if (!safeAddOne || !safeHash32 || !safeInvokeCallback || !safeFillBytes || !safeAllocBytes) {
        throw new Error("expected benchmark exports to be present");
      }

      const rawAddOne = rawModule.bindFunction({
        symbol: "add_one",
        result: "int32_t",
        params: ["int32_t"],
      });

      const callbackHandle = safeInvokeCallback.registerCallback("cbctx_invoke_i32", (value: unknown) => {
        if (typeof value !== "number") {
          throw new Error("expected numeric callback value");
        }
        return value + 1;
      });

      warmup(20_000, () => {
        safeAddOne.call(41);
      });
      warmup(20_000, () => {
        rawAddOne.call(41);
      });
      warmup(5_000, () => {
        safeHash32.call(payload, BigInt(payload.length));
      });
      warmup(2_000, () => {
        safeHash32.call(largePayload, BigInt(largePayload.length));
      });
      warmup(5_000, () => {
        safeFillBytes.call(outBuffer, BigInt(outBuffer.length));
      });
      warmup(5_000, () => {
        safeInvokeCallback.call(41, callbackHandle.pointer, null);
      });
      warmup(2_000, () => {
        const owned = safeAllocBytes.call(8n) as {
          decodeBytes(length: number): Uint8Array;
          dispose(): void;
        };
        owned.decodeBytes(8);
        owned.dispose();
      });

      const safeScalar = runTimedSamples("safe scalar call add_one", 100_000, 9, () => {
        safeAddOne.call(41);
      });
      const rawScalar = runTimedSamples("raw scalar call add_one", 100_000, 9, () => {
        rawAddOne.call(41);
      });
      const borrowedBuffer = runTimedSamples("safe borrowed buffer call hash32", 40_000, 9, () => {
        safeHash32.call(payload, BigInt(payload.length));
      });
      const largeBorrowedBuffer = runTimedSamples("safe borrowed buffer call hash32 (4 KiB)", 8_000, 9, () => {
        safeHash32.call(largePayload, BigInt(largePayload.length));
      });
      const outBufferWrite = runTimedSamples("safe out buffer call fill_bytes", 40_000, 9, () => {
        safeFillBytes.call(outBuffer, BigInt(outBuffer.length));
      });
      const callbackRoundTrip = runTimedSamples("safe callback roundtrip invoke_i32_callback", 30_000, 9, () => {
        safeInvokeCallback.call(41, callbackHandle.pointer, null);
      });
      const ownedPointerRoundTrip = runTimedSamples("safe owned pointer alloc/decode/dispose", 8_000, 9, () => {
        const owned = safeAllocBytes.call(8n) as {
          decodeBytes(length: number): Uint8Array;
          dispose(): void;
        };
        owned.decodeBytes(8);
        owned.dispose();
      });

      console.log("j2fz benchmark environment");
      console.log(`  node ${process.version}`);
      console.log(`  platform ${process.platform} ${process.arch}`);
      console.log(`  compiler ${process.env.CC ?? "cc"}`);
      console.log("");
      for (const summary of [
        moduleLoad,
        safeScalar,
        rawScalar,
        borrowedBuffer,
        largeBorrowedBuffer,
        outBufferWrite,
        callbackRoundTrip,
        ownedPointerRoundTrip,
      ]) {
        console.log(formatSummary(summary));
        console.log("");
      }

      callbackHandle.dispose();
    } finally {
      safeModule.dispose();
      rawModule.dispose();
    }
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
}

main();
