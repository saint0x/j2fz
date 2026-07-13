import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { parseAbiManifest } from "../src/runtime/manifest.js";
import {
  renderBindingJavaScript,
  renderBindingModule,
  renderBindingTypes,
  renderGeneratedPackageJson,
  renderGeneratedReadme,
} from "../src/generator/render.js";
import { writeGeneratedBindings } from "../src/generator/write.js";

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
  ],
});

test("renderBindingModule emits typed bindings", () => {
  const text = renderBindingModule(SAMPLE_MANIFEST, {
    runtimeImportPath: "j2fz",
  });

  assert.match(text, /import \{ fileURLToPath \} from "node:url";/);
  assert.match(text, /const ABI_MANIFEST: FozzyAbiManifest = \{/);
  assert.match(text, /export function createBindings/);
  assert.match(text, /export function createBindings\(options: Omit<LoadEmbeddedModuleOptions, "manifest">\): demo_bridgeBindings \{/);
  assert.match(text, /export function createDiscoveredBindings\(options: Omit<LoadPackageOptions, "discovery"> = \{\}\)/);
  assert.match(text, /export interface UserRow \{/);
  assert.match(text, /id: bigint;/);
  assert.match(text, /score: number;/);
  assert.match(text, /export const TaskState = \{/);
  assert.match(text, /Ready: 1,/);
  assert.match(text, /export type TaskState = \(typeof TaskState\)\[keyof typeof TaskState\];/);
  assert.match(text, /hash32\(ptr_borrowed: Uint8Array \| Buffer, len: bigint\): number;/);
  assert.match(text, /compute_async\(value: number\): Promise<number>;/);
  assert.match(text, /export interface uint8_tOwnedHandle extends OpaqueHandle<"uint8_t">/);
  assert.match(text, /decodeBytes\(length: number\): Uint8Array;/);
  assert.match(text, /alloc_bytes\(len: bigint\): uint8_tOwnedHandle;/);
  assert.match(text, /export interface with_callback_mainRegisteredCallbackHandle extends RegisteredCallbackHandle<"main"> \{\}/);
  assert.match(text, /export type with_callback_mainCallbackContext = CallbackContextValue<"void">;/);
  assert.match(text, /with_callback\(cb: with_callback_mainRegisteredCallbackHandle, cb_ctx: CallbackContextValue<"void">, value: number\): number;/);
  assert.match(text, /dispose\(\): void;/);
  assert.match(text, /register_with_callback_main\(fn: \(value: number, ctx: OpaqueHandle<"void">\) => number\): with_callback_mainRegisteredCallbackHandle;/);
  assert.match(text, /module\.exports\.get\("hash32"\)/);
  assert.match(text, /loadFozzyModuleWithManifest\(\{ \.\.\.options, manifest: ABI_MANIFEST \}\)/);
});

test("renderBindingJavaScript emits publishable runtime wrapper", () => {
  const text = renderBindingJavaScript(SAMPLE_MANIFEST, {
    runtimeImportPath: "j2fz",
  });

  assert.match(text, /import \{ fileURLToPath \} from "node:url";/);
  assert.match(text, /import \{ loadFozzyModuleWithManifest, loadFozzyPackageWithManifest \} from "j2fz";/);
  assert.match(text, /const ABI_MANIFEST = \{/);
  assert.match(text, /export const TaskState = \{/);
  assert.match(text, /export function createBindings\(options\)/);
  assert.match(text, /export function createDiscoveredBindings\(options = \{\}\)/);
  assert.match(text, /loadFozzyPackageWithManifest\(\{/);
  assert.match(text, /packageRoot: fileURLToPath\(new URL\("\.", import\.meta\.url\)\)/);
  assert.match(text, /return fn\.call\(ptr_borrowed, len\);/);
  assert.match(text, /dispose\(\) \{/);
  assert.match(text, /register_with_callback_main\(fn\) \{/);
  assert.match(text, /registerCallback\("main", fn\)/);
});

test("renderBindingTypes emits declaration output", () => {
  const text = renderBindingTypes(SAMPLE_MANIFEST, {
    runtimeImportPath: "j2fz",
  });

  assert.match(text, /import type \{ CallbackContextValue, LoadEmbeddedModuleOptions, LoadPackageOptions, OpaqueHandle, RegisteredCallbackHandle \} from "j2fz";/);
  assert.match(text, /export interface UserRow \{/);
  assert.match(text, /export const TaskState = \{/);
  assert.match(text, /export interface demo_bridgeBindings/);
  assert.match(text, /alloc_bytes\(len: bigint\): uint8_tOwnedHandle;/);
  assert.match(text, /compute_async\(value: number\): Promise<number>;/);
  assert.match(text, /with_callback\(cb: with_callback_mainRegisteredCallbackHandle, cb_ctx: CallbackContextValue<"void">, value: number\): number;/);
  assert.match(text, /register_with_callback_main\(fn: \(value: number, ctx: OpaqueHandle<"void">\) => number\): with_callback_mainRegisteredCallbackHandle;/);
  assert.match(text, /export function createBindings\(options: Omit<LoadEmbeddedModuleOptions, "manifest">\): demo_bridgeBindings;/);
  assert.match(text, /export function createDiscoveredBindings\(options\?: Omit<LoadPackageOptions, "discovery">\): demo_bridgeBindings;/);
});

test("renderGeneratedPackageJson emits dependency-aware package metadata", () => {
  const text = renderGeneratedPackageJson(SAMPLE_MANIFEST, {
    runtimeImportPath: "j2fz",
  });

  assert.match(text, /"name": "demo\.bridge-j2fz"/);
  assert.match(text, /"main": "\.\/index\.js"/);
  assert.match(text, /"types": "\.\/index\.d\.ts"/);
  assert.doesNotMatch(text, /abi\.manifest\.json/);
  assert.match(text, /"j2fz": "\*"/);
});

test("renderGeneratedReadme emits export inventory", () => {
  const text = renderGeneratedReadme(SAMPLE_MANIFEST);

  assert.match(text, /Generated j2fz bindings for the Fozzy package `demo\.bridge`\./);
  assert.match(text, /`hash32\(`ptr_borrowed: Uint8Array \| Buffer`/);
  assert.match(text, /compute_async\(`value: number`\) => Promise<number>/);
  assert.match(text, /## Callback Registrations/);
  assert.match(text, /`register_with_callback_main\(fn: \(value: number, ctx: OpaqueHandle<"void">\) => number\)`/);
  assert.match(text, /import \{ createBindings, createDiscoveredBindings \} from "\.";/);
  assert.match(text, /const discovered = createDiscoveredBindings\(\);/);
  assert.match(text, /const bindings = createBindings\(\{/);
  assert.doesNotMatch(text, /abiManifest/);
});

test("writeGeneratedBindings writes a package-ready output directory", () => {
  const outDir = mkdtempSync(join(tmpdir(), "j2fz-generated-"));
  const result = writeGeneratedBindings(outDir, SAMPLE_MANIFEST, {
    runtimeImportPath: "j2fz",
  });

  assert.equal(result.packageDir, outDir);
  assert.equal(readFileSync(result.modulePath, "utf8").includes("loadFozzyModuleWithManifest"), true);
  assert.equal(readFileSync(result.typesPath, "utf8").includes("OpaqueHandle"), true);
  assert.equal(readFileSync(result.packageJsonPath, "utf8").includes('"main": "./index.js"'), true);
  assert.equal(result.readmePath !== null, true);
  assert.equal(readFileSync(result.modulePath, "utf8").includes("ABI_MANIFEST"), true);
  assert.equal(readFileSync(result.readmePath!, "utf8").includes("## Usage"), true);
  assert.equal(readFileSync(join(outDir, "index.ts"), "utf8").includes("LoadEmbeddedModuleOptions"), true);
});
