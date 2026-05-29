export type JsHostHandle = bigint;

export type JsHostOwnedValue =
  | null
  | boolean
  | number
  | bigint
  | string
  | Uint8Array
  | Record<string, unknown>
  | unknown[]
  | ((...args: unknown[]) => unknown)
  | Promise<unknown>;

export interface JsHostModuleRecord {
  readonly kind: "module";
  readonly specifier: string;
  readonly namespace: Record<string, unknown>;
}

export interface JsHostExportRecord {
  readonly kind: "export";
  readonly moduleHandle: JsHostHandle;
  readonly exportName: string;
  readonly value: unknown;
}

export interface JsHostValueRecord {
  readonly kind: "value";
  readonly value: JsHostOwnedValue;
}

export interface JsHostCallbackRegistrationRecord {
  readonly kind: "callback_registration";
  readonly name: string;
  readonly callback: (value: number) => number;
}

export type JsHostRecord =
  | JsHostModuleRecord
  | JsHostExportRecord
  | JsHostValueRecord
  | JsHostCallbackRegistrationRecord;

export interface JsHostRuntimeOptions {
  resolveModuleSpecifier?: (specifier: string) => Promise<string>;
  resolveModuleNamespace?: (specifier: string) => Promise<Record<string, unknown> | null>;
}

export interface JsHostUtf8View {
  readonly bytes: Uint8Array;
  readonly length: number;
}
