import test from "node:test";
import assert from "node:assert/strict";

import { parseAbiManifest } from "../src/runtime/manifest.js";

const SAMPLE_MANIFEST = {
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
};

test("parseAbiManifest accepts a valid manifest", () => {
  const manifest = parseAbiManifest(SAMPLE_MANIFEST);
  assert.equal(manifest.package.name, "demo.bridge");
  assert.equal(manifest.exports[0]?.name, "hash32");
});

test("parseAbiManifest rejects missing struct fields", () => {
  const broken = structuredClone(SAMPLE_MANIFEST) as typeof SAMPLE_MANIFEST;
  const firstLayout = broken.reprCLayouts[0];
  if (!firstLayout) {
    throw new Error("expected reprC layout");
  }
  broken.reprCLayouts[0] = { ...firstLayout, fields: [] };
  assert.throws(() => parseAbiManifest(broken), /must include fields/);
});
