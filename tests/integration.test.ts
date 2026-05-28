import test from "node:test";
import assert from "node:assert/strict";
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";

import { loadFozzyModule } from "../src/runtime/loader.js";
import { loadFozzyPackage } from "../src/runtime/discovery.js";
import { defaultSharedLibraryFileNameForPackage } from "../src/runtime/platform.js";
import { OwnershipError, ResourceStateError, SymbolLoadError } from "../src/runtime/errors.js";

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

function compileFixtureLibrary(root: string): { libraryPath: string; manifestPath: string } {
  const sourcePath = join(root, "fixture.c");
  const libraryPath = join(root, sharedLibraryName("fixture"));
  const manifestPath = join(root, "fixture.abi.json");

  const source = `
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct UserRow {
  uint64_t id;
  uint32_t score;
} UserRow;

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

UserRow make_user_row(uint64_t id, uint32_t score) {
  UserRow row;
  row.id = id;
  row.score = score;
  return row;
}

uint32_t sum_user_row(UserRow row) {
  return (uint32_t)(row.id + (uint64_t)row.score);
}
`;
  writeFileSync(sourcePath, source, "utf8");

  const manifest = {
    schemaVersion: "fozzylang.ffi_abi.v1",
    package: {
      name: "fixture.bridge",
      version: "0.0.1",
    },
    abiRevision: 1,
    targetTriple: process.arch,
    dataLayoutHash: "fixture-layout",
    compilerIdentityHash: "fixture-cc",
    panicBoundary: "error",
    layoutPolicy: {
      reprCStableOnly: true,
      nonReprCUnstable: true,
    },
    symbolVersioning: "strict-name-signature-v1",
    contractSchema: "fozzylang.ffi_contracts.v1",
    reprCLayouts: [
      {
        name: "UserRow",
        kind: "struct",
        size: 16,
        align: 8,
        fields: [
          { name: "id", c: "uint64_t" },
          { name: "score", c: "uint32_t" },
        ],
      },
    ],
    exports: [
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
      {
        name: "make_user_row",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "id",
            fzy: "u64",
            c: "uint64_t",
            contract: {
              ownership: "value",
              nullability: "n/a",
              mutability: "const",
              lifetimeAnchor: null,
              view: null,
            },
          },
          {
            name: "score",
            fzy: "u32",
            c: "uint32_t",
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
          fzy: "UserRow",
          c: "UserRow",
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
        name: "sum_user_row",
        async: false,
        symbolVersion: 1,
        params: [
          {
            name: "row",
            fzy: "UserRow",
            c: "UserRow",
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
    ],
  };
  writeFileSync(manifestPath, JSON.stringify(manifest, null, 2), "utf8");

  const compiler = process.env.CC ?? "cc";
  const args =
    process.platform === "darwin"
      ? ["-dynamiclib", "-O2", "-o", libraryPath, sourcePath]
      : process.platform === "win32"
        ? ["/LD", sourcePath, `/Fe:${libraryPath}`]
        : ["-shared", "-fPIC", "-O2", "-o", libraryPath, sourcePath];

  const result = spawnSync(compiler, args, { encoding: "utf8" });
  if (result.status !== 0) {
    throw new Error(`fixture compile failed: ${result.stderr || result.stdout}`);
  }

  return { libraryPath, manifestPath };
}

function materializeGeneratedPackageLayout(
  root: string,
  input: { libraryPath: string; manifestPath: string },
  packageName: string,
): { packageRoot: string; libraryPath: string; manifestPath: string } {
  const nativeDir = join(root, "native");
  mkdirSync(nativeDir, { recursive: true });
  const packageLibraryPath = join(nativeDir, defaultSharedLibraryFileNameForPackage(packageName));
  const packageManifestPath = join(root, "abi.manifest.json");
  copyFileSync(input.libraryPath, packageLibraryPath);
  copyFileSync(input.manifestPath, packageManifestPath);
  return {
    packageRoot: root,
    libraryPath: packageLibraryPath,
    manifestPath: packageManifestPath,
  };
}

test("loadFozzyModule binds a real native library and callback", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-fixture-"));
  try {
    const { libraryPath, manifestPath } = compileFixtureLibrary(root);
    const events: string[] = [];
    const module = loadFozzyModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "fixture.bridge",
        version: "0.0.1",
      },
      ownedPointerReleasers: {
        alloc_bytes: "alloc_bytes_free",
      },
      diagnostics: {
        onEvent(event) {
          events.push(event.kind);
        },
      },
    });

    const hash32 = module.exports.get("hash32");
    assert.ok(hash32);
    const hashValue = hash32.call(Buffer.from("abc", "utf8"), BigInt(3));
    assert.equal(typeof hashValue, "number");

    const invoke = module.exports.get("invoke_i32_callback");
    assert.ok(invoke);
    const callback = invoke.registerCallback("cbctx_invoke_i32", (value: unknown) => {
      assert.equal(value, 41);
      return 42;
    });
    const callbackValue = invoke.call(41, callback.pointer, 0);
    assert.equal(callbackValue, 42);
    callback.dispose();

    const fill = module.exports.get("fill_bytes");
    assert.ok(fill);
    const out = Buffer.alloc(4);
    const fillResult = fill.call(out, BigInt(out.length));
    assert.equal(fillResult, 4);
    assert.deepEqual([...out], [1, 2, 3, 4]);

    const alloc = module.exports.get("alloc_bytes");
    assert.ok(alloc);
    const owned = alloc.call(BigInt(4)) as {
      decodeBytes(length: number): Uint8Array;
      dispose(): void;
      disposed: boolean;
    };
    assert.deepEqual([...owned.decodeBytes(4)], [65, 66, 67, 68]);
    assert.equal(owned.disposed, false);
    owned.dispose();
    assert.equal(owned.disposed, true);

    const makeUserRow = module.exports.get("make_user_row");
    assert.ok(makeUserRow);
    const row = makeUserRow.call(7n, 5) as { id: bigint; score: number };
    assert.equal(row.id, 7n);
    assert.equal(row.score, 5);

    const sumUserRow = module.exports.get("sum_user_row");
    assert.ok(sumUserRow);
    assert.equal(sumUserRow.call({ id: 7n, score: 5 }), 12);

    module.dispose();
    assert.ok(events.includes("sync.call.started"));
    assert.ok(events.includes("sync.call.completed"));
    assert.ok(events.includes("callback.registered"));
    assert.ok(events.includes("callback.invoked"));
    assert.ok(events.includes("callback.completed"));
    assert.ok(events.includes("callback.disposed"));
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("loadFozzyPackage discovers generated package artifacts", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-package-"));
  try {
    const compiled = compileFixtureLibrary(root);
    const packageLayout = materializeGeneratedPackageLayout(root, compiled, "fixture.bridge");
    const events: string[] = [];

    const module = loadFozzyPackage({
      discovery: {
        packageRoot: packageLayout.packageRoot,
        package: {
          name: "fixture.bridge",
          version: "0.0.1",
        },
      },
      package: {
        name: "fixture.bridge",
        version: "0.0.1",
      },
      ownedPointerReleasers: {
        alloc_bytes: "alloc_bytes_free",
      },
      diagnostics: {
        onEvent(event) {
          events.push(event.kind);
        },
      },
    });

    const hash32 = module.exports.get("hash32");
    assert.ok(hash32);
    const hashValue = hash32.call(Buffer.from("pkg", "utf8"), BigInt(3));
    assert.equal(typeof hashValue, "number");

    module.dispose();
    assert.ok(events.includes("package.discovery.resolved"));
    assert.ok(events.includes("module.load.start"));
    assert.ok(events.includes("module.export.bound"));
    assert.ok(events.includes("module.disposed"));
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("loadFozzyModule rejects manifest exports whose symbols are missing", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-missing-symbol-"));
  try {
    const { libraryPath, manifestPath } = compileFixtureLibrary(root);
    const broken = JSON.parse(readFileSync(manifestPath, "utf8")) as Record<string, unknown>;
    const exports = broken.exports as Array<Record<string, unknown>>;
    const hashExport = exports.find((item) => item.name === "hash32");
    if (!hashExport) {
      throw new Error("expected hash32 export in fixture manifest");
    }
    hashExport.name = "missing_hash32";
    writeFileSync(manifestPath, `${JSON.stringify(broken, null, 2)}\n`, "utf8");

    assert.throws(
      () =>
        loadFozzyModule({
          paths: {
            sharedLibrary: libraryPath,
            abiManifest: manifestPath,
          },
          package: {
            name: "fixture.bridge",
            version: "0.0.1",
          },
        }),
      (error: unknown) => {
        assert.ok(error instanceof SymbolLoadError);
        assert.match(error.message, /failed binding export missing_hash32|failed to bind/i);
        return true;
      },
    );
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("loadFozzyModule rejects use after module disposal and keeps callback disposal idempotent", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-disposed-"));
  try {
    const { libraryPath, manifestPath } = compileFixtureLibrary(root);
    const events: string[] = [];
    const module = loadFozzyModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "fixture.bridge",
        version: "0.0.1",
      },
      diagnostics: {
        onEvent(event) {
          events.push(event.kind);
        },
      },
    });

    const invoke = module.exports.get("invoke_i32_callback");
    assert.ok(invoke);
    const callback = invoke.registerCallback("cbctx_invoke_i32", (value: unknown) => value);
    assert.equal(callback.disposed, false);
    callback.dispose();
    callback.dispose();
    assert.equal(callback.disposed, true);
    assert.equal(events.filter((event) => event === "callback.disposed").length, 1);

    const hash32 = module.exports.get("hash32");
    assert.ok(hash32);
    module.dispose();
    module.dispose();

    assert.throws(() => hash32.call(Buffer.from("x", "utf8"), 1n), ResourceStateError);
    assert.throws(
      () =>
        invoke.registerCallback(
          "cbctx_invoke_i32",
          (() => 0) as unknown as (...args: unknown[]) => unknown,
        ),
      ResourceStateError,
    );
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("loadFozzyModule rejects non-function callback registration values", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-callback-type-"));
  try {
    const { libraryPath, manifestPath } = compileFixtureLibrary(root);
    const module = loadFozzyModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "fixture.bridge",
        version: "0.0.1",
      },
    });

    try {
      const invoke = module.exports.get("invoke_i32_callback");
      assert.ok(invoke);
      assert.throws(
        () =>
          invoke.registerCallback(
            "cbctx_invoke_i32",
            "not a function" as unknown as (...args: unknown[]) => unknown,
          ),
        OwnershipError,
      );
    } finally {
      module.dispose();
    }
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
