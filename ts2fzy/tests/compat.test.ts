import test from "node:test";
import assert from "node:assert/strict";

import { assertAbiCompatible, compareAbiManifests } from "../src/runtime/compat.js";
import { parseAbiManifest } from "../src/runtime/manifest.js";

function makeManifest() {
  return parseAbiManifest({
    schemaVersion: "fozzylang.ffi_abi.v1",
    package: {
      name: "fixture.bridge",
      version: "0.0.1",
    },
    abiRevision: 1,
    targetTriple: "aarch64-apple-darwin",
    dataLayoutHash: "abc",
    compilerIdentityHash: "def",
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
    ],
  });
}

test("compareAbiManifests accepts identical manifests", () => {
  const baseline = makeManifest();
  const current = makeManifest();
  const report = compareAbiManifests(current, baseline);
  assert.equal(report.ok, true);
  assert.deepEqual(report.issues, []);
});

test("compareAbiManifests allows additive exports", () => {
  const baseline = makeManifest();
  const current = makeManifest();
  current.exports.push({
    name: "extra_export",
    async: false,
    symbolVersion: 1,
    params: [],
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
  });
  const report = compareAbiManifests(current, baseline);
  assert.equal(report.ok, true);
  assert.deepEqual(report.addedExports, ["extra_export"]);
});

test("assertAbiCompatible rejects changed signatures", () => {
  const baseline = makeManifest();
  const current = makeManifest();
  const first = current.exports[0];
  if (!first) {
    throw new Error("missing export");
  }
  first.return.c = "uint64_t";
  assert.throws(() => assertAbiCompatible(current, baseline), /signature changed/);
});

test("assertAbiCompatible rejects contract changes", () => {
  const baseline = makeManifest();
  const current = makeManifest();
  const first = current.exports[0];
  if (!first) {
    throw new Error("missing export");
  }
  const firstParam = first.params[0];
  if (!firstParam) {
    throw new Error("missing param");
  }
  firstParam.contract.nullability = "nullable";
  assert.throws(() => assertAbiCompatible(current, baseline), /parameter contract changed/);
});
