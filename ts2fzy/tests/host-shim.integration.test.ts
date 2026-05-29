import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFileSync } from "node:child_process";
import koffi from "koffi";

import { spawnJsHostServerProcess } from "../src/host/process.js";

function compileHostShim(root: string): string {
  const output = join(root, "libj2fz_host_shim.dylib");
  execFileSync(
    "clang",
    [
      "-shared",
      "-fPIC",
      "-O2",
      "-Wall",
      "-Wextra",
      "-o",
      output,
      join(process.cwd(), "native", "j2fz_js_host_shim.c"),
      "-lpthread",
    ],
    { cwd: join(process.cwd()), stdio: "pipe" },
  );
  return output;
}

function writeModuleFixture(root: string): string {
  const modulePath = join(root, "fixture.mjs");
  writeFileSync(
    modulePath,
    [
      "export function addOne(value) { return value + 1; }",
      "export async function doubleLater(value) { return value * 2; }",
      "export function greet(name) { return `hi ${name}`; }",
    ].join("\n"),
    "utf8",
  );
  return modulePath;
}

test("native JS host shim bridges modules, values, await, and native callbacks", async () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-host-shim-"));
  try {
    const socketPath = join(root, "host.sock");
    const modulePath = writeModuleFixture(root);
    const server = await spawnJsHostServerProcess({ socketPath });
    const libraryPath = compileHostShim(root);

    process.env.J2FZ_JS_HOST_SOCKET = socketPath;

    const lib = koffi.load(libraryPath);
    const moduleOpen = lib.func("int32_t j2fz_js_module_open(const uint8_t *spec_borrowed, size_t len, _Out_ uint64_t *module_out, void *module_ctx)");
    const moduleClose = lib.func("int32_t j2fz_js_module_close(uint64_t module_handle)");
    const exportGet = lib.func("int32_t j2fz_js_export_get(uint64_t module_handle, const uint8_t *export_borrowed, size_t len, _Out_ uint64_t *export_out, void *export_ctx)");
    const exportRelease = lib.func("int32_t j2fz_js_export_release(uint64_t export_handle)");
    const valueI32 = lib.func("int32_t j2fz_js_value_i32(int32_t value, _Out_ uint64_t *value_out, size_t value_len)");
    const valueStr = lib.func("int32_t j2fz_js_value_str(const uint8_t *value_borrowed, size_t len, _Out_ uint64_t *value_out, size_t value_len)");
    const valueAsI32 = lib.func("int32_t j2fz_js_value_as_i32(uint64_t value_handle, _Out_ int32_t *value_out, size_t value_len)");
    const valueAsUtf8 = lib.func("int32_t j2fz_js_value_as_utf8(uint64_t value_handle, _Out_ uint64_t *utf8_ptr_out, size_t utf8_ptr_len, _Out_ size_t *utf8_len_out, size_t utf8_len_len)");
    const valueCopyUtf8 = lib.func("int32_t j2fz_js_value_copy_utf8(uint64_t value_handle, _Out_ uint8_t *ptr_out, size_t len)");
    const valueRelease = lib.func("int32_t j2fz_js_value_release(uint64_t value_handle)");
    const callFn = lib.func("int32_t j2fz_js_call(uint64_t function_handle, const uint64_t *argv_borrowed, size_t argv_len, _Out_ uint64_t *result_out, size_t result_len)");
    const awaitFn = lib.func("int32_t j2fz_js_await(uint64_t value_handle, _Out_ uint64_t *result_out, size_t result_len)");
    const registerHost = lib.func(
      "int32_t j2fz_js_host_register_i32_i32(const uint8_t *name_borrowed, size_t len, void *cb, void *cb_ctx, _Out_ uint64_t *registration_out, void *registration_ctx)",
    );
    const releaseHost = lib.func("int32_t j2fz_js_host_registration_release(uint64_t registration_handle)");
    const callbackPtrFactory = lib.func("void *j2fz_test_add_three_i32_ptr(void)");

    const moduleOut = [0n];
    assert.equal(moduleOpen(Buffer.from(modulePath, "utf8"), BigInt(Buffer.byteLength(modulePath)), moduleOut, null), 0);
    const moduleHandle = BigInt(moduleOut[0] as bigint);

    const addExportOut = [0n];
    assert.equal(exportGet(moduleHandle, Buffer.from("addOne", "utf8"), 6n, addExportOut, null), 0);
    const addHandle = BigInt(addExportOut[0] as bigint);

    const inputOut = [0n];
    assert.equal(valueI32(41, inputOut, 1n), 0);
    const inputHandle = BigInt(inputOut[0] as bigint);

    const resultOut = [0n];
    assert.equal(callFn(addHandle, [inputHandle], 1n, resultOut, 1n), 0);
    const resultHandle = BigInt(resultOut[0] as bigint);

    const resultValue = [0];
    assert.equal(valueAsI32(resultHandle, resultValue, 1n), 0);
    assert.equal(resultValue[0], 42);

    const greetExportOut = [0n];
    assert.equal(exportGet(moduleHandle, Buffer.from("greet", "utf8"), 5n, greetExportOut, null), 0);
    const greetHandle = BigInt(greetExportOut[0] as bigint);

    const nameOut = [0n];
    assert.equal(valueStr(Buffer.from("fozzy", "utf8"), 5n, nameOut, 1n), 0);
    const nameHandle = BigInt(nameOut[0] as bigint);

    const greetResultOut = [0n];
    assert.equal(callFn(greetHandle, [nameHandle], 1n, greetResultOut, 1n), 0);
    const greetResultHandle = BigInt(greetResultOut[0] as bigint);

    const utf8PtrOut = [0n];
    const utf8LenOut = [0n];
    assert.equal(valueAsUtf8(greetResultHandle, utf8PtrOut, 1n, utf8LenOut, 1n), 0);
    const greetBuffer = Buffer.alloc(Number(utf8LenOut[0]));
    assert.equal(valueCopyUtf8(greetResultHandle, greetBuffer, BigInt(greetBuffer.length)), 0);
    assert.equal(greetBuffer.toString("utf8"), "hi fozzy");

    const asyncExportOut = [0n];
    assert.equal(exportGet(moduleHandle, Buffer.from("doubleLater", "utf8"), 11n, asyncExportOut, null), 0);
    const asyncHandle = BigInt(asyncExportOut[0] as bigint);
    const pendingOut = [0n];
    assert.equal(callFn(asyncHandle, [inputHandle], 1n, pendingOut, 1n), 0);
    const pendingHandle = BigInt(pendingOut[0] as bigint);
    const settledOut = [0n];
    assert.equal(awaitFn(pendingHandle, settledOut, 1n), 0);
    const settledHandle = BigInt(settledOut[0] as bigint);
    const settledValue = [0];
    assert.equal(valueAsI32(settledHandle, settledValue, 1n), 0);
    assert.equal(settledValue[0], 82);

    const callbackPtr = callbackPtrFactory();
    const registrationOut = [0n];
    assert.equal(
      registerHost(Buffer.from("plus_three", "utf8"), 10n, callbackPtr, null, registrationOut, null),
      0,
    );
    const registrationHandle = BigInt(registrationOut[0] as bigint);

    const hostModuleOut = [0n];
    assert.equal(moduleOpen(Buffer.from("j2fz:host", "utf8"), 9n, hostModuleOut, null), 0);
    const hostModuleHandle = BigInt(hostModuleOut[0] as bigint);
    const plusThreeExportOut = [0n];
    assert.equal(exportGet(hostModuleHandle, Buffer.from("plus_three", "utf8"), 10n, plusThreeExportOut, null), 0);
    const plusThreeHandle = BigInt(plusThreeExportOut[0] as bigint);
    const callbackResultOut = [0n];
    assert.equal(callFn(plusThreeHandle, [inputHandle], 1n, callbackResultOut, 1n), 0);
    const callbackPendingHandle = BigInt(callbackResultOut[0] as bigint);
    const callbackSettledOut = [0n];
    assert.equal(awaitFn(callbackPendingHandle, callbackSettledOut, 1n), 0);
    const callbackResultHandle = BigInt(callbackSettledOut[0] as bigint);
    const callbackValueOut = [0];
    assert.equal(valueAsI32(callbackResultHandle, callbackValueOut, 1n), 0);
    assert.equal(callbackValueOut[0], 44);

    assert.equal(releaseHost(registrationHandle), 0);
    assert.equal(valueRelease(inputHandle), 0);
    assert.equal(valueRelease(resultHandle), 0);
    assert.equal(valueRelease(nameHandle), 0);
    assert.equal(valueRelease(greetResultHandle), 0);
    assert.equal(valueRelease(pendingHandle), 0);
    assert.equal(valueRelease(settledHandle), 0);
    assert.equal(valueRelease(callbackPendingHandle), 0);
    assert.equal(valueRelease(callbackResultHandle), 0);
    assert.equal(exportRelease(addHandle), 0);
    assert.equal(exportRelease(greetHandle), 0);
    assert.equal(exportRelease(asyncHandle), 0);
    assert.equal(exportRelease(plusThreeHandle), 0);
    assert.equal(moduleClose(hostModuleHandle), 0);
    assert.equal(moduleClose(moduleHandle), 0);

    await server.close();
  } finally {
    delete process.env.J2FZ_JS_HOST_SOCKET;
    rmSync(root, { recursive: true, force: true });
  }
});
