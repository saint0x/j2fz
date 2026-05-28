import test from "node:test";
import assert from "node:assert/strict";

import type { DiagnosticEmitter } from "../src/runtime/diagnostics.js";
import { AsyncInteropError } from "../src/runtime/errors.js";
import { invokeAsyncHandleRuntimeBinding } from "../src/runtime/loader.js";

test("invokeAsyncHandleRuntimeBinding resolves and drops handle", async () => {
  const calls: string[] = [];
  const value = await invokeAsyncHandleRuntimeBinding(
    {
      start(_input: unknown, handleOut: Array<bigint | number | null>) {
        calls.push("start");
        handleOut[0] = 99n;
        return 0;
      },
      poll(handle, doneOut) {
        calls.push(`poll:${String(handle)}`);
        doneOut[0] = 1;
        return 0;
      },
      awaitResult(handle, resultOut) {
        calls.push(`await:${String(handle)}`);
        resultOut[0] = 42;
        return 0;
      },
      drop(handle) {
        calls.push(`drop:${String(handle)}`);
        return 0;
      },
    },
    "demo_async",
    [41],
    1,
    100,
  );

  assert.equal(value, 42);
  assert.deepEqual(calls, ["start", "poll:99", "await:99", "drop:99"]);
});

test("invokeAsyncHandleRuntimeBinding rejects on timeout and drops handle", async () => {
  const calls: string[] = [];
  await assert.rejects(
    () =>
      invokeAsyncHandleRuntimeBinding(
        {
          start(handleOut: Array<bigint | number | null>) {
            calls.push("start");
            handleOut[0] = 5n;
            return 0;
          },
          poll(handle, doneOut) {
            calls.push(`poll:${String(handle)}`);
            doneOut[0] = 0;
            return 0;
          },
          awaitResult() {
            calls.push("await");
            return 0;
          },
          drop(handle) {
            calls.push(`drop:${String(handle)}`);
            return 0;
          },
        },
        "demo_async",
        [],
        1,
        10,
      ),
    (error: unknown) => {
      assert.ok(error instanceof AsyncInteropError);
      assert.match(error.message, /timed out/);
      return true;
    },
  );

  assert.ok(calls.includes("drop:5"));
  assert.ok(calls.some((entry) => entry.startsWith("poll:")));
});

test("invokeAsyncHandleRuntimeBinding emits lifecycle diagnostics", async () => {
  const events: string[] = [];
  const emit: DiagnosticEmitter = Object.assign(
    (event: { kind: string }) => {
      events.push(event.kind);
    },
    { enabled: true as const },
  );
  await invokeAsyncHandleRuntimeBinding(
    {
      start(handleOut: Array<bigint | number | null>) {
        handleOut[0] = 7n;
        return 0;
      },
      poll(_handle, doneOut) {
        doneOut[0] = 1;
        return 0;
      },
      awaitResult(_handle, resultOut) {
        resultOut[0] = 99;
        return 0;
      },
      drop() {
        return 0;
      },
    },
    "demo_async",
    [],
    1,
    100,
    emit,
  );

  assert.deepEqual(events, ["async.started", "async.completed"]);
});
