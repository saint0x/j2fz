import test from "node:test";
import assert from "node:assert/strict";

import { parseAbiManifest } from "../src/runtime/manifest.js";
import { renderBindingModule } from "../src/generator/render.js";

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
});

test("renderBindingModule emits typed bindings", () => {
  const text = renderBindingModule(SAMPLE_MANIFEST, {
    runtimeImportPath: "j2fz",
  });

  assert.match(text, /export function createBindings/);
  assert.match(text, /hash32\(ptr_borrowed: Uint8Array \| Buffer, len: bigint\): number;/);
  assert.match(text, /export interface uint8_tOwnedHandle extends OpaqueHandle<"uint8_t">/);
  assert.match(text, /decodeBytes\(length: number\): Uint8Array;/);
  assert.match(text, /alloc_bytes\(len: bigint\): uint8_tOwnedHandle;/);
  assert.match(text, /module\.exports\.get\("hash32"\)/);
});
