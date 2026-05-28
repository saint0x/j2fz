import test from "node:test";
import assert from "node:assert/strict";

import type { AbiCallbackBinding, FozzyAbiManifest } from "../src/types/abi.js";
import { parseAbiManifest } from "../src/runtime/manifest.js";

const SAMPLE_MANIFEST: FozzyAbiManifest = {
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

test("parseAbiManifest rejects async flag and execution mismatches", () => {
  const broken = structuredClone(SAMPLE_MANIFEST) as typeof SAMPLE_MANIFEST;
  const firstExport = broken.exports[0];
  if (!firstExport) {
    throw new Error("expected export");
  }
  broken.exports[0] = {
    ...firstExport,
    async: true,
    contract: {
      ...firstExport.contract,
      execution: "sync",
      asyncBoundary: null,
    },
  };

  assert.throws(() => parseAbiManifest(broken), /async flag does not match execution contract/);
});

test("parseAbiManifest rejects callback bindings that reference missing params", () => {
  const broken = structuredClone(SAMPLE_MANIFEST);
  const firstExport = broken.exports[0];
  if (!firstExport) {
    throw new Error("expected export");
  }
  const missingCallbackBinding: AbiCallbackBinding = {
    callbackParam: "missing_callback",
    contextParam: null,
    bindingId: "main",
    obligation: "register before use",
    signature: {
      returnCType: "int32_t",
      params: [{ name: "value", c: "int32_t" }],
    },
    lifetime: "registered",
  };
  broken.exports[0] = {
    ...firstExport,
    contract: {
      ...firstExport.contract,
      callbackBindings: [missingCallbackBinding],
    },
  };

  assert.throws(() => parseAbiManifest(broken), /references missing param missing_callback/);
});

test("parseAbiManifest rejects async boundary result type mismatches", () => {
  const broken = structuredClone(SAMPLE_MANIFEST);
  const firstExport = broken.exports[0];
  if (!firstExport) {
    throw new Error("expected export");
  }
  broken.exports[0] = {
    ...firstExport,
    name: "hash32_async",
    async: true,
    contract: {
      execution: "async-handle-v1",
      callbackBindings: [],
      asyncBoundary: {
        model: "async-handle-v1",
        startSymbol: "hash32_async_start",
        pollSymbol: "hash32_async_poll",
        awaitSymbol: "hash32_async_await",
        dropSymbol: "hash32_async_drop",
        resultType: "int32_t",
      },
    },
  };

  assert.throws(() => parseAbiManifest(broken), /asyncBoundary\.resultType must match return type/);
});

test("parseAbiManifest rejects callback bindings with nullable callback params", () => {
  const broken = structuredClone(SAMPLE_MANIFEST);
  broken.exports[0] = {
    name: "with_callback",
    async: false,
    symbolVersion: 1,
    params: [
      {
        name: "cb",
        fzy: "fn(i32) -> i32",
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
          callbackParam: "cb",
          contextParam: null,
          bindingId: "main",
          obligation: "register before use",
          signature: {
            returnCType: "int32_t",
            params: [{ name: "value", c: "int32_t" }],
          },
          lifetime: "registered",
        },
      ],
      asyncBoundary: null,
    },
  };

  assert.throws(() => parseAbiManifest(broken), /must be non_null/);
});

test("parseAbiManifest rejects callback bindings whose context signature type mismatches", () => {
  const broken = structuredClone(SAMPLE_MANIFEST);
  broken.exports[0] = {
    name: "with_callback",
    async: false,
    symbolVersion: 1,
    params: [
      {
        name: "cb",
        fzy: "fn(i32) -> i32",
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
        c: "uint8_t*",
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
          callbackParam: "cb",
          contextParam: "cb_ctx",
          bindingId: "main",
          obligation: "register before use",
          signature: {
            returnCType: "int32_t",
            params: [
              { name: "value", c: "int32_t" },
              { name: "ctx", c: "void*" },
            ],
          },
          lifetime: "registered",
        },
      ],
      asyncBoundary: null,
    },
  };

  assert.throws(() => parseAbiManifest(broken), /context signature type must match context param/);
});
