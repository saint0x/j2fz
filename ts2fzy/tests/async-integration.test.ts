import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";

import { loadFozzyModule } from "../src/runtime/loader.js";

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

function compileAsyncFixture(root: string): { libraryPath: string; manifestPath: string } {
  const sourcePath = join(root, "async_fixture.c");
  const libraryPath = join(root, sharedLibraryName("async_fixture"));
  const manifestPath = join(root, "async_fixture.abi.json");

  writeFileSync(
    sourcePath,
    `
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct hash_entry {
  uint64_t handle;
  uint32_t result;
  int polls;
  int active;
} hash_entry_t;

typedef struct bytes_entry {
  uint64_t handle;
  uint8_t *result;
  int polls;
  int active;
} bytes_entry_t;

static hash_entry_t hash_entries[16];
static bytes_entry_t bytes_entries[16];
static uint64_t next_handle = 1;

static hash_entry_t *find_hash_entry(uint64_t handle) {
  for (size_t i = 0; i < 16; i++) {
    if (hash_entries[i].active && hash_entries[i].handle == handle) {
      return &hash_entries[i];
    }
  }
  return NULL;
}

static bytes_entry_t *find_bytes_entry(uint64_t handle) {
  for (size_t i = 0; i < 16; i++) {
    if (bytes_entries[i].active && bytes_entries[i].handle == handle) {
      return &bytes_entries[i];
    }
  }
  return NULL;
}

static uint32_t hash32_impl(const uint8_t *ptr, size_t len) {
  uint32_t acc = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    acc ^= ptr[i];
    acc *= 16777619u;
  }
  return acc;
}

int32_t hash32_async_start(const uint8_t *ptr, size_t len, uint64_t *handle_out) {
  if (ptr == NULL || handle_out == NULL) return -1;
  for (size_t i = 0; i < 16; i++) {
    if (!hash_entries[i].active) {
      hash_entries[i].active = 1;
      hash_entries[i].handle = next_handle++;
      hash_entries[i].result = hash32_impl(ptr, len);
      hash_entries[i].polls = 0;
      *handle_out = hash_entries[i].handle;
      return 0;
    }
  }
  return -2;
}

int32_t hash32_async_poll(uint64_t handle, int32_t *done_out) {
  hash_entry_t *entry = find_hash_entry(handle);
  if (entry == NULL || done_out == NULL) return -1;
  entry->polls += 1;
  *done_out = entry->polls >= 2 ? 1 : 0;
  return 0;
}

int32_t hash32_async_await(uint64_t handle, uint32_t *result_out) {
  hash_entry_t *entry = find_hash_entry(handle);
  if (entry == NULL || result_out == NULL) return -1;
  *result_out = entry->result;
  return 0;
}

int32_t hash32_async_drop(uint64_t handle) {
  hash_entry_t *entry = find_hash_entry(handle);
  if (entry == NULL) return -1;
  entry->active = 0;
  return 0;
}

int32_t alloc_bytes_async_start(size_t len, uint64_t *handle_out) {
  if (handle_out == NULL) return -1;
  for (size_t i = 0; i < 16; i++) {
    if (!bytes_entries[i].active) {
      uint8_t *buf = (uint8_t *)malloc(len + 1);
      if (buf == NULL) return -2;
      for (size_t j = 0; j < len; j++) {
        buf[j] = (uint8_t)('a' + (int)j);
      }
      buf[len] = 0;
      bytes_entries[i].active = 1;
      bytes_entries[i].handle = next_handle++;
      bytes_entries[i].result = buf;
      bytes_entries[i].polls = 0;
      *handle_out = bytes_entries[i].handle;
      return 0;
    }
  }
  return -3;
}

int32_t alloc_bytes_async_poll(uint64_t handle, int32_t *done_out) {
  bytes_entry_t *entry = find_bytes_entry(handle);
  if (entry == NULL || done_out == NULL) return -1;
  entry->polls += 1;
  *done_out = entry->polls >= 2 ? 1 : 0;
  return 0;
}

int32_t alloc_bytes_async_await(uint64_t handle, uint8_t **result_out) {
  bytes_entry_t *entry = find_bytes_entry(handle);
  if (entry == NULL || result_out == NULL) return -1;
  *result_out = entry->result;
  entry->result = NULL;
  return 0;
}

int32_t alloc_bytes_async_drop(uint64_t handle) {
  bytes_entry_t *entry = find_bytes_entry(handle);
  if (entry == NULL) return -1;
  if (entry->result != NULL) {
    free(entry->result);
    entry->result = NULL;
  }
  entry->active = 0;
  return 0;
}

void alloc_bytes_async_free(uint8_t *ptr) {
  free(ptr);
}
`,
    "utf8",
  );

  writeFileSync(
    manifestPath,
    JSON.stringify(
      {
        schemaVersion: "fozzylang.ffi_abi.v1",
        package: {
          name: "async.fixture",
          version: "0.0.1",
        },
        abiRevision: 1,
        targetTriple: process.arch,
        dataLayoutHash: "async-layout",
        compilerIdentityHash: "async-cc",
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
            name: "hash32_async",
            async: true,
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
              execution: "async-handle-v1",
              callbackBindings: [],
              asyncBoundary: {
                model: "async-handle-v1",
                startSymbol: "hash32_async_start",
                pollSymbol: "hash32_async_poll",
                awaitSymbol: "hash32_async_await",
                dropSymbol: "hash32_async_drop",
                resultType: "uint32_t",
              },
            },
          },
          {
            name: "alloc_bytes_async",
            async: true,
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
              execution: "async-handle-v1",
              callbackBindings: [],
              asyncBoundary: {
                model: "async-handle-v1",
                startSymbol: "alloc_bytes_async_start",
                pollSymbol: "alloc_bytes_async_poll",
                awaitSymbol: "alloc_bytes_async_await",
                dropSymbol: "alloc_bytes_async_drop",
                resultType: "uint8_t*",
              },
            },
          },
        ],
      },
      null,
      2,
    ),
    "utf8",
  );

  const compiler = process.env.CC ?? "cc";
  const args =
    process.platform === "darwin"
      ? ["-dynamiclib", "-O2", "-o", libraryPath, sourcePath]
      : process.platform === "win32"
        ? ["/LD", sourcePath, `/Fe:${libraryPath}`]
        : ["-shared", "-fPIC", "-O2", "-o", libraryPath, sourcePath];
  const result = spawnSync(compiler, args, { encoding: "utf8" });
  if (result.status !== 0) {
    throw new Error(`async fixture compile failed: ${result.stderr || result.stdout}`);
  }

  return { libraryPath, manifestPath };
}

test("loadFozzyModule adapts async borrowed inputs and resolves scalar results", async () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-async-"));
  try {
    const { libraryPath, manifestPath } = compileAsyncFixture(root);
    const events: string[] = [];
    const module = loadFozzyModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "async.fixture",
        version: "0.0.1",
      },
      pollIntervalMs: 1,
      asyncTimeoutMs: 100,
      ownedPointerReleasers: {
        alloc_bytes_async: "alloc_bytes_async_free",
      },
      diagnostics: {
        mode: "debug",
        onEvent(event) {
          events.push(event.kind);
        },
      },
    });

    try {
      const hashAsync = module.exports.get("hash32_async");
      assert.ok(hashAsync);
      const result = await hashAsync.call("hello async", 11n);
      assert.equal(typeof result, "number");
      assert.ok(events.includes("async.started"));
      assert.ok(events.includes("async.completed"));
    } finally {
      module.dispose();
    }
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("loadFozzyModule adapts async owned pointer returns", async () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-async-owned-"));
  try {
    const { libraryPath, manifestPath } = compileAsyncFixture(root);
    const module = loadFozzyModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "async.fixture",
        version: "0.0.1",
      },
      pollIntervalMs: 1,
      asyncTimeoutMs: 100,
      ownedPointerReleasers: {
        alloc_bytes_async: "alloc_bytes_async_free",
      },
    });

    try {
      const allocAsync = module.exports.get("alloc_bytes_async");
      assert.ok(allocAsync);
      const handle = (await allocAsync.call(3n)) as {
        decodeBytes(length: number): Uint8Array;
        dispose(): void;
        disposed: boolean;
      };
      assert.deepEqual([...handle.decodeBytes(3)], [97, 98, 99]);
      handle.dispose();
      assert.equal(handle.disposed, true);
    } finally {
      module.dispose();
    }
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
