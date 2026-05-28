import test from "node:test";
import assert from "node:assert/strict";
import koffi from "koffi";

import { parseAbiManifest } from "../src/runtime/manifest.js";
import { buildRuntimeTypeRegistry, declareKoffiFunction, koffiTypeFromCType } from "../src/runtime/koffi.js";

const SAMPLE_MANIFEST = parseAbiManifest({
  schemaVersion: "fozzylang.ffi_abi.v1",
  package: {
    name: "demo.bridge",
    version: "1.2.3",
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
  ],
});

test("koffi type registry builds repr(C) layouts", () => {
  koffi.reset();
  const registry = buildRuntimeTypeRegistry(SAMPLE_MANIFEST);
  assert.ok(registry.layouts.has("UserRow"));
});

test("koffi declares function signatures from ABI metadata", () => {
  koffi.reset();
  const registry = buildRuntimeTypeRegistry(SAMPLE_MANIFEST);
  const abiExport = SAMPLE_MANIFEST.exports[0];
  assert.ok(abiExport);
  const decl = declareKoffiFunction(abiExport, registry);
  assert.equal(decl.symbol, "hash32");
  assert.equal(decl.params.length, 2);
});

test("koffiTypeFromCType rejects unknown named types", () => {
  assert.throws(
    () => koffiTypeFromCType("MissingType", new Map()),
    /unsupported C type/,
  );
});
