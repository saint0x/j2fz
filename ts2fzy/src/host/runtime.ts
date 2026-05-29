import { pathToFileURL } from "node:url";

import type {
  JsHostCallbackRegistrationRecord,
  JsHostExportRecord,
  JsHostHandle,
  JsHostModuleRecord,
  JsHostOwnedValue,
  JsHostRecord,
  JsHostRuntimeOptions,
  JsHostUtf8View,
  JsHostValueRecord,
} from "./types.js";

function toRecord(namespaceValue: unknown): Record<string, unknown> {
  if (typeof namespaceValue !== "object" || namespaceValue === null) {
    throw new TypeError("Imported module namespace must be an object");
  }
  return namespaceValue as Record<string, unknown>;
}

function normalizeModulePath(specifier: string): string {
  if (
    specifier.startsWith("file://")
    || specifier.startsWith("node:")
    || specifier.startsWith("data:")
  ) {
    return specifier;
  }
  if (specifier.startsWith("/") || specifier.startsWith("./") || specifier.startsWith("../")) {
    return pathToFileURL(specifier).href;
  }
  return specifier;
}

function cloneBytes(value: Uint8Array): Uint8Array {
  return new Uint8Array(value);
}

function normalizeValue(value: unknown): JsHostOwnedValue {
  if (
    value === null
    || typeof value === "boolean"
    || typeof value === "number"
    || typeof value === "bigint"
    || typeof value === "string"
  ) {
    return value;
  }
  if (typeof value === "function") {
    return value as (...args: unknown[]) => unknown;
  }
  if (value instanceof Uint8Array) {
    return cloneBytes(value);
  }
  if (value instanceof Promise) {
    return value;
  }
  if (Array.isArray(value)) {
    return [...value];
  }
  if (typeof value === "object" && value !== null) {
    return { ...(value as Record<string, unknown>) };
  }
  throw new TypeError(`Unsupported JS host value type: ${typeof value}`);
}

function expectRecord<TKind extends JsHostRecord["kind"]>(
  record: JsHostRecord | undefined,
  kind: TKind,
  handle: JsHostHandle,
): Extract<JsHostRecord, { kind: TKind }> {
  if (record === undefined) {
    throw new Error(`Unknown host handle: ${handle.toString()}`);
  }
  if (record.kind !== kind) {
    throw new Error(`Host handle ${handle.toString()} is not a ${kind}`);
  }
  return record as Extract<JsHostRecord, { kind: TKind }>;
}

function normalizeI32(value: number): number {
  if (!Number.isInteger(value)) {
    throw new TypeError("Expected i32-compatible integer");
  }
  if (value < -2147483648 || value > 2147483647) {
    throw new RangeError("Value is outside i32 range");
  }
  return value;
}

function normalizeFunction(value: unknown): (...args: unknown[]) => unknown {
  if (typeof value !== "function") {
    throw new TypeError("JS host export is not callable");
  }
  return value as (...args: unknown[]) => unknown;
}

export class JsHostRuntime {
  readonly #records = new Map<JsHostHandle, JsHostRecord>();
  readonly #utf8 = new TextEncoder();
  readonly #options: JsHostRuntimeOptions;
  #nextHandle = 1n;

  constructor(options: JsHostRuntimeOptions = {}) {
    this.#options = options;
  }

  async openModule(specifier: string): Promise<JsHostHandle> {
    const virtualNamespace = this.#options.resolveModuleNamespace !== undefined
      ? await this.#options.resolveModuleNamespace(specifier)
      : null;
    const namespace = virtualNamespace ?? toRecord(
      await import(
        this.#options.resolveModuleSpecifier !== undefined
          ? await this.#options.resolveModuleSpecifier(specifier)
          : normalizeModulePath(specifier)
      ),
    );
    return this.#storeModule({
      kind: "module",
      specifier,
      namespace,
    });
  }

  closeModule(moduleHandle: JsHostHandle): void {
    const record = expectRecord(this.#records.get(moduleHandle), "module", moduleHandle);
    this.#records.delete(moduleHandle);
    for (const [handle, value] of this.#records.entries()) {
      if (value.kind === "export" && value.moduleHandle === moduleHandle) {
        this.#records.delete(handle);
      }
    }
    void record;
  }

  getExport(moduleHandle: JsHostHandle, exportName: string): JsHostHandle {
    const moduleRecord = expectRecord(this.#records.get(moduleHandle), "module", moduleHandle);
    if (!(exportName in moduleRecord.namespace)) {
      throw new Error(`Module export not found: ${exportName}`);
    }
    return this.#storeExport({
      kind: "export",
      moduleHandle,
      exportName,
      value: moduleRecord.namespace[exportName],
    });
  }

  releaseExport(exportHandle: JsHostHandle): void {
    expectRecord(this.#records.get(exportHandle), "export", exportHandle);
    this.#records.delete(exportHandle);
  }

  releaseValue(valueHandle: JsHostHandle): void {
    expectRecord(this.#records.get(valueHandle), "value", valueHandle);
    this.#records.delete(valueHandle);
  }

  registerI32I32(name: string, callback: (value: number) => number): JsHostHandle {
    return this.#storeRegistration({
      kind: "callback_registration",
      name,
      callback(value: number): number {
        return normalizeI32(callback(normalizeI32(value)));
      },
    });
  }

  releaseRegistration(registrationHandle: JsHostHandle): void {
    expectRecord(this.#records.get(registrationHandle), "callback_registration", registrationHandle);
    this.#records.delete(registrationHandle);
  }

  invokeRegisteredI32I32(registrationHandle: JsHostHandle, value: number): number {
    const record = expectRecord(
      this.#records.get(registrationHandle),
      "callback_registration",
      registrationHandle,
    );
    return record.callback(value);
  }

  createNull(): JsHostHandle {
    return this.#storeValue({ kind: "value", value: null });
  }

  createBool(value: boolean): JsHostHandle {
    return this.#storeValue({ kind: "value", value });
  }

  createI32(value: number): JsHostHandle {
    return this.#storeValue({ kind: "value", value: normalizeI32(value) });
  }

  createU64(value: bigint): JsHostHandle {
    if (value < 0n) {
      throw new RangeError("u64 value must be non-negative");
    }
    return this.#storeValue({ kind: "value", value });
  }

  createF64(value: number): JsHostHandle {
    return this.#storeValue({ kind: "value", value });
  }

  createString(value: string): JsHostHandle {
    return this.#storeValue({ kind: "value", value });
  }

  createBytes(value: Uint8Array): JsHostHandle {
    return this.#storeValue({ kind: "value", value: cloneBytes(value) });
  }

  asBool(valueHandle: JsHostHandle): boolean {
    const value = this.#loadValue(valueHandle);
    if (typeof value !== "boolean") {
      throw new TypeError("Host value is not a boolean");
    }
    return value;
  }

  asI32(valueHandle: JsHostHandle): number {
    const value = this.#loadValue(valueHandle);
    if (typeof value !== "number") {
      throw new TypeError("Host value is not an i32-compatible number");
    }
    return normalizeI32(value);
  }

  asU64(valueHandle: JsHostHandle): bigint {
    const value = this.#loadValue(valueHandle);
    if (typeof value !== "bigint") {
      throw new TypeError("Host value is not a u64-compatible bigint");
    }
    if (value < 0n) {
      throw new RangeError("Host bigint is negative");
    }
    return value;
  }

  asF64(valueHandle: JsHostHandle): number {
    const value = this.#loadValue(valueHandle);
    if (typeof value !== "number") {
      throw new TypeError("Host value is not an f64-compatible number");
    }
    return value;
  }

  asUtf8(valueHandle: JsHostHandle): JsHostUtf8View {
    const value = this.#loadValue(valueHandle);
    if (typeof value !== "string") {
      throw new TypeError("Host value is not a string");
    }
    const bytes = this.#utf8.encode(value);
    return {
      bytes,
      length: bytes.byteLength,
    };
  }

  copyUtf8(valueHandle: JsHostHandle): Uint8Array {
    return cloneBytes(this.asUtf8(valueHandle).bytes);
  }

  call(functionHandle: JsHostHandle, argvHandles: readonly JsHostHandle[]): JsHostHandle {
    const fn = normalizeFunction(this.#loadCallable(functionHandle));
    const args = argvHandles.map((handle) => this.#loadValue(handle));
    return this.#storeValue({
      kind: "value",
      value: normalizeValue(fn(...args)),
    });
  }

  async awaitValue(valueHandle: JsHostHandle): Promise<JsHostHandle> {
    const value = this.#loadValue(valueHandle);
    if (!(value instanceof Promise)) {
      return this.#storeValue({ kind: "value", value: normalizeValue(value) });
    }
    return this.#storeValue({
      kind: "value",
      value: normalizeValue(await value),
    });
  }

  inspectValue(valueHandle: JsHostHandle): JsHostOwnedValue {
    return normalizeValue(this.#loadValue(valueHandle));
  }

  #loadCallable(functionHandle: JsHostHandle): unknown {
    const record = this.#records.get(functionHandle);
    if (record === undefined) {
      throw new Error(`Unknown host handle: ${functionHandle.toString()}`);
    }
    if (record.kind === "export") {
      return record.value;
    }
    if (record.kind === "value") {
      return record.value;
    }
    throw new Error(`Host handle ${functionHandle.toString()} is not callable`);
  }

  #loadValue(valueHandle: JsHostHandle): JsHostOwnedValue {
    const record = expectRecord(this.#records.get(valueHandle), "value", valueHandle);
    return record.value;
  }

  #storeModule(record: JsHostModuleRecord): JsHostHandle {
    return this.#store(record);
  }

  #storeExport(record: JsHostExportRecord): JsHostHandle {
    return this.#store(record);
  }

  #storeValue(record: JsHostValueRecord): JsHostHandle {
    return this.#store(record);
  }

  #storeRegistration(record: JsHostCallbackRegistrationRecord): JsHostHandle {
    return this.#store(record);
  }

  #store(record: JsHostRecord): JsHostHandle {
    const handle = this.#nextHandle;
    this.#nextHandle += 1n;
    this.#records.set(handle, record);
    return handle;
  }
}
