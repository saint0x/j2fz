import test from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";

import { JsHostRuntime } from "../src/host/runtime.js";

async function writeFixtureModule(): Promise<string> {
  const root = await mkdtemp(path.join(tmpdir(), "j2fz-host-runtime-"));
  const modulePath = path.join(root, "fixture.mjs");
  await writeFile(
    modulePath,
    [
      "export const label = 'host-ok';",
      "export function addOne(value) { return value + 1; }",
      "export async function doubleLater(value) { return value * 2; }",
    ].join("\n"),
    "utf8",
  );
  return modulePath;
}

test("JsHostRuntime opens modules and calls exports", async () => {
  const runtime = new JsHostRuntime();
  const modulePath = await writeFixtureModule();

  const moduleHandle = await runtime.openModule(modulePath);
  const addOneHandle = runtime.getExport(moduleHandle, "addOne");
  const inputHandle = runtime.createI32(41);
  const resultHandle = runtime.call(addOneHandle, [inputHandle]);

  assert.equal(runtime.asI32(resultHandle), 42);

  runtime.releaseValue(inputHandle);
  runtime.releaseValue(resultHandle);
  runtime.releaseExport(addOneHandle);
  runtime.closeModule(moduleHandle);
});

test("JsHostRuntime supports awaitable values", async () => {
  const runtime = new JsHostRuntime();
  const modulePath = await writeFixtureModule();

  const moduleHandle = await runtime.openModule(modulePath);
  const asyncHandle = runtime.getExport(moduleHandle, "doubleLater");
  const inputHandle = runtime.createI32(9);
  const pendingHandle = runtime.call(asyncHandle, [inputHandle]);
  const settledHandle = await runtime.awaitValue(pendingHandle);

  assert.equal(runtime.asI32(settledHandle), 18);

  runtime.releaseValue(inputHandle);
  runtime.releaseValue(pendingHandle);
  runtime.releaseValue(settledHandle);
  runtime.releaseExport(asyncHandle);
  runtime.closeModule(moduleHandle);
});

test("JsHostRuntime supports strings, bytes, and registered callbacks", async () => {
  const runtime = new JsHostRuntime();

  const stringHandle = runtime.createString("hello");
  const utf8 = runtime.asUtf8(stringHandle);
  assert.equal(utf8.length, 5);
  assert.deepEqual([...runtime.copyUtf8(stringHandle)], [104, 101, 108, 108, 111]);

  const bytesHandle = runtime.createBytes(new Uint8Array([1, 2, 3]));
  const inspected = runtime.inspectValue(bytesHandle);
  assert.ok(inspected instanceof Uint8Array);
  assert.deepEqual([...inspected], [1, 2, 3]);

  const registration = runtime.registerI32I32("plus_three", (value) => value + 3);
  assert.equal(runtime.invokeRegisteredI32I32(registration, 4), 7);

  runtime.releaseValue(stringHandle);
  runtime.releaseValue(bytesHandle);
  runtime.releaseRegistration(registration);
});
