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

function compileFixtureLibrary(root: string): { libraryPath: string; manifestPath: string } {
  const sourcePath = join(root, "fixture.c");
  const libraryPath = join(root, sharedLibraryName("fixture"));
  const manifestPath = join(root, "fixture.abi.json");

  const source = `
#include <stddef.h>
#include <stdint.h>

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
    reprCLayouts: [],
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

test("loadFozzyModule binds a real native library and callback", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-fixture-"));
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
    module.dispose();
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
