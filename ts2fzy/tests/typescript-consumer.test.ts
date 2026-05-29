import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, relative } from "node:path";
import { spawnSync } from "node:child_process";

import { parseAbiManifest } from "../src/runtime/manifest.js";
import { writeGeneratedBindings } from "../src/generator/write.js";

const SAMPLE_MANIFEST = parseAbiManifest({
  schemaVersion: "fozzylang.ffi_abi.v1",
  package: {
    name: "consumer.bridge",
    version: "1.0.0",
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
    {
      name: "TaskState",
      kind: "enum",
      size: 4,
      align: 4,
      variants: [
        { name: "Ready", value: 1 },
        { name: "Busy", value: 2 },
      ],
      storage: "int32_t",
    },
  ],
  exports: [
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
      name: "compute_async",
      async: true,
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
        execution: "async-handle-v1",
        callbackBindings: [],
        asyncBoundary: {
          model: "async-handle-v1",
          startSymbol: "compute_async_start",
          pollSymbol: "compute_async_poll",
          awaitSymbol: "compute_async_await",
          dropSymbol: "compute_async_drop",
          resultType: "int32_t",
        },
      },
    },
    {
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
          c: "void*",
          contract: {
            ownership: "borrowed",
            nullability: "nullable",
            mutability: "const",
            lifetimeAnchor: null,
            view: null,
          },
        },
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
        callbackBindings: [
          {
            callbackParam: "cb",
            contextParam: "cb_ctx",
            bindingId: "main",
            obligation: "register a callback before use",
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
    },
  ],
});

test("generated bindings type-check in a consumer-style TypeScript project", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-ts-consumer-"));
  const runtimeImportPath = relative(root, join(process.cwd(), "src", "index.js")).replace(/\\/g, "/");

  writeGeneratedBindings(root, SAMPLE_MANIFEST, {
    runtimeImportPath: runtimeImportPath.startsWith(".") ? runtimeImportPath : `./${runtimeImportPath}`,
  });
  unlinkSync(join(root, "index.ts"));

  writeFileSync(
    join(root, "consumer.ts"),
    `import {
  TaskState,
  createBindings,
  createDiscoveredBindings,
  type UserRow,
  type with_callback_mainCallbackContext,
  type with_callback_mainRegisteredCallbackHandle,
} from "./index.js";

declare const row: UserRow;
const rowId: bigint = row.id;
const rowScore: number = row.score;
const state: TaskState = TaskState.Ready;

declare const bindings: ReturnType<typeof createBindings>;
declare const callbackHandle: with_callback_mainRegisteredCallbackHandle;
declare const callbackContext: with_callback_mainCallbackContext;
const asyncValue: Promise<number> = bindings.compute_async(42);

bindings.dispose();
bindings.with_callback(callbackHandle, callbackContext, 42);
bindings.register_with_callback_main((value, ctx) => {
  const next: number = value + (ctx ? 1 : 0);
  return next;
}).dispose();

createDiscoveredBindings();
void asyncValue;
void rowId;
void rowScore;
void state;
`,
    "utf8",
  );

  writeFileSync(
    join(root, "tsconfig.json"),
    JSON.stringify(
      {
        compilerOptions: {
          target: "ES2022",
          module: "NodeNext",
          moduleResolution: "NodeNext",
          strict: true,
          noEmit: true,
          exactOptionalPropertyTypes: true,
          noUncheckedIndexedAccess: true,
          skipLibCheck: true,
          types: ["node"],
          typeRoots: [join(process.cwd(), "node_modules", "@types")],
        },
        include: ["consumer.ts", "index.d.ts"],
      },
      null,
      2,
    ),
    "utf8",
  );

  const tsc = join(process.cwd(), "node_modules", ".bin", process.platform === "win32" ? "tsc.cmd" : "tsc");
  const result = spawnSync(tsc, ["-p", join(root, "tsconfig.json")], {
    encoding: "utf8",
  });

  assert.equal(result.status, 0, result.stderr || result.stdout);
});
