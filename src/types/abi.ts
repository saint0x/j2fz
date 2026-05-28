export type AbiSchemaVersion = "fozzylang.ffi_abi.v1";

export type PanicBoundary = "abort" | "error";

export type OwnershipKind = "value" | "borrowed" | "owned" | "out" | "inout";
export type NullabilityKind = "n/a" | "nullable" | "non_null";
export type MutabilityKind = "const" | "mut";

export interface AbiPackageIdentity {
  name: string;
  version: string;
}

export interface AbiPointerView {
  kind: "ptr_len";
  lengthParam: string;
}

export interface AbiParamContract {
  ownership: OwnershipKind;
  nullability: NullabilityKind;
  mutability: MutabilityKind;
  lifetimeAnchor: string | null;
  view: AbiPointerView | null;
}

export interface AbiReturnContract {
  ownership: OwnershipKind;
  nullability: NullabilityKind;
  mutability: MutabilityKind;
}

export interface AbiCallbackTypeParam {
  name: string;
  c: string;
}

export interface AbiCallbackType {
  convention?: "cdecl" | "stdcall";
  returnCType: string;
  params: AbiCallbackTypeParam[];
}

export interface AbiCallbackBinding {
  callbackParam: string;
  contextParam: string | null;
  bindingId: string;
  obligation: string;
  signature: AbiCallbackType;
  lifetime: "transient" | "registered";
}

export interface AbiAsyncBoundary {
  model: "async-handle-v1";
  startSymbol: string;
  pollSymbol: string;
  awaitSymbol: string;
  dropSymbol: string;
  resultType: string;
}

export interface AbiExportContract {
  execution: "sync" | "async-handle-v1";
  callbackBindings: AbiCallbackBinding[];
  asyncBoundary: AbiAsyncBoundary | null;
}

export interface AbiParam {
  name: string;
  fzy: string;
  c: string;
  contract: AbiParamContract;
}

export interface AbiReturn {
  fzy: string;
  c: string;
  contract: AbiReturnContract;
}

export interface AbiField {
  name: string;
  c: string;
}

export interface AbiEnumVariant {
  name: string;
  value: number | bigint;
}

export interface AbiReprCLayout {
  name: string;
  kind: "struct" | "enum";
  size: number;
  align: number;
  fields?: AbiField[];
  variants?: AbiEnumVariant[];
  storage?: string;
}

export interface AbiExport {
  name: string;
  async: boolean;
  symbolVersion: number;
  params: AbiParam[];
  return: AbiReturn;
  contract: AbiExportContract;
}

export interface FozzyAbiManifest {
  schemaVersion: AbiSchemaVersion;
  package: AbiPackageIdentity;
  abiRevision: number;
  targetTriple: string;
  dataLayoutHash: string;
  compilerIdentityHash: string;
  panicBoundary: PanicBoundary;
  layoutPolicy: {
    reprCStableOnly: boolean;
    nonReprCUnstable: boolean;
  };
  symbolVersioning: string;
  contractSchema: string;
  reprCLayouts: AbiReprCLayout[];
  exports: AbiExport[];
}
