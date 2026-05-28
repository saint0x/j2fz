import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";

import { loadUnsafeRawModule } from "../src/runtime/raw.js";
import { SymbolLoadError } from "../src/runtime/errors.js";

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

function compileRawFixture(root: string): { libraryPath: string; manifestPath: string } {
  const sourcePath = join(root, "raw_fixture.c");
  const libraryPath = join(root, sharedLibraryName("raw_fixture"));
  const manifestPath = join(root, "raw_fixture.abi.json");

  writeFileSync(
    sourcePath,
    `
#include <stddef.h>
#include <stdint.h>

int32_t add_one(int32_t value) {
  return value + 1;
}

int32_t write_value(int32_t *out_value) {
  if (out_value == NULL) return -1;
  *out_value = 77;
  return 0;
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
          name: "raw.fixture",
          version: "0.0.1",
        },
        abiRevision: 1,
        targetTriple: process.arch,
        dataLayoutHash: "raw-layout",
        compilerIdentityHash: "raw-cc",
        panicBoundary: "error",
        layoutPolicy: {
          reprCStableOnly: true,
          nonReprCUnstable: true,
        },
        symbolVersioning: "strict-name-signature-v1",
        contractSchema: "fozzylang.ffi_contracts.v1",
        reprCLayouts: [],
        exports: [],
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
    throw new Error(`raw fixture compile failed: ${result.stderr || result.stdout}`);
  }

  return { libraryPath, manifestPath };
}

test("loadUnsafeRawModule binds explicit raw symbols", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-raw-"));
  try {
    const { libraryPath, manifestPath } = compileRawFixture(root);
    const module = loadUnsafeRawModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "raw.fixture",
        version: "0.0.1",
      },
    });

    const addOne = module.bindFunction({
      symbol: "add_one",
      result: "int32_t",
      params: ["int32_t"],
    });
    assert.equal(addOne.call(41), 42);

    const writeValue = module.bindFunction({
      symbol: "write_value",
      result: "int32_t",
      params: ["_Out_ int32_t *"],
    });
    const out = [0];
    assert.equal(writeValue.call(out), 0);
    assert.equal(out[0], 77);

    module.dispose();
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("loadUnsafeRawModule rejects missing raw symbols cleanly", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-raw-missing-"));
  try {
    const { libraryPath, manifestPath } = compileRawFixture(root);
    const module = loadUnsafeRawModule({
      paths: {
        sharedLibrary: libraryPath,
        abiManifest: manifestPath,
      },
      package: {
        name: "raw.fixture",
        version: "0.0.1",
      },
    });

    assert.throws(
      () =>
        module.bindFunction({
          symbol: "does_not_exist",
          result: "int32_t",
          params: [],
        }),
      (error: unknown) => {
        assert.ok(error instanceof SymbolLoadError);
        assert.match(error.message, /failed binding raw symbol does_not_exist/);
        return true;
      },
    );

    module.dispose();
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
