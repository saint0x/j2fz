import type {
  AbiCallbackBinding,
  AbiCallbackType,
  AbiExport,
  AbiExportContract,
  AbiField,
  AbiParam,
  AbiParamContract,
  AbiReprCLayout,
  AbiReturn,
  AbiReturnContract,
  FozzyAbiManifest,
  PanicBoundary,
} from "../types/abi.js";
import { AbiParseError, AbiValidationError } from "./errors.js";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function expectRecord(value: unknown, path: string): Record<string, unknown> {
  if (!isRecord(value)) {
    throw new AbiParseError(`${path} must be an object`);
  }
  return value;
}

function expectString(value: unknown, path: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new AbiParseError(`${path} must be a non-empty string`);
  }
  return value;
}

function expectBoolean(value: unknown, path: string): boolean {
  if (typeof value !== "boolean") {
    throw new AbiParseError(`${path} must be a boolean`);
  }
  return value;
}

function expectNumber(value: unknown, path: string): number {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    throw new AbiParseError(`${path} must be a finite number`);
  }
  return value;
}

function expectInteger(value: unknown, path: string): number {
  const parsed = expectNumber(value, path);
  if (!Number.isInteger(parsed)) {
    throw new AbiParseError(`${path} must be an integer`);
  }
  return parsed;
}

function expectArray(value: unknown, path: string): unknown[] {
  if (!Array.isArray(value)) {
    throw new AbiParseError(`${path} must be an array`);
  }
  return value;
}

function optionalString(value: unknown, path: string): string | null {
  if (value === null || value === undefined) {
    return null;
  }
  return expectString(value, path);
}

function expectPanicBoundary(value: unknown, path: string): PanicBoundary {
  const boundary = expectString(value, path);
  if (boundary !== "abort" && boundary !== "error") {
    throw new AbiParseError(`${path} must be "abort" or "error"`);
  }
  return boundary;
}

function parseParamContract(input: unknown, path: string): AbiParamContract {
  const record = expectRecord(input, path);
  const ownership = expectString(record.ownership, `${path}.ownership`);
  const nullability = expectString(record.nullability, `${path}.nullability`);
  const mutability = expectString(record.mutability, `${path}.mutability`);
  const lifetimeAnchor = optionalString(record.lifetimeAnchor, `${path}.lifetimeAnchor`);
  let view = null;

  if (record.view !== null && record.view !== undefined) {
    const viewRecord = expectRecord(record.view, `${path}.view`);
    const kind = expectString(viewRecord.kind, `${path}.view.kind`);
    if (kind !== "ptr_len") {
      throw new AbiParseError(`${path}.view.kind must be "ptr_len"`);
    }
    view = {
      kind,
      lengthParam: expectString(viewRecord.lengthParam, `${path}.view.lengthParam`),
    } as const;
  }

  if (
    ownership !== "value" &&
    ownership !== "borrowed" &&
    ownership !== "owned" &&
    ownership !== "out" &&
    ownership !== "inout"
  ) {
    throw new AbiParseError(`${path}.ownership is unsupported`);
  }
  if (nullability !== "n/a" && nullability !== "nullable" && nullability !== "non_null") {
    throw new AbiParseError(`${path}.nullability is unsupported`);
  }
  if (mutability !== "const" && mutability !== "mut") {
    throw new AbiParseError(`${path}.mutability is unsupported`);
  }

  return {
    ownership,
    nullability,
    mutability,
    lifetimeAnchor,
    view,
  };
}

function parseReturnContract(input: unknown, path: string): AbiReturnContract {
  const record = expectRecord(input, path);
  const ownership = expectString(record.ownership, `${path}.ownership`);
  const nullability = expectString(record.nullability, `${path}.nullability`);
  const mutability = expectString(record.mutability, `${path}.mutability`);

  if (
    ownership !== "value" &&
    ownership !== "borrowed" &&
    ownership !== "owned" &&
    ownership !== "out" &&
    ownership !== "inout"
  ) {
    throw new AbiParseError(`${path}.ownership is unsupported`);
  }
  if (nullability !== "n/a" && nullability !== "nullable" && nullability !== "non_null") {
    throw new AbiParseError(`${path}.nullability is unsupported`);
  }
  if (mutability !== "const" && mutability !== "mut") {
    throw new AbiParseError(`${path}.mutability is unsupported`);
  }

  return {
    ownership,
    nullability,
    mutability,
  };
}

function parseCallbackType(input: unknown, path: string): AbiCallbackType {
  const record = expectRecord(input, path);
  const params = expectArray(record.params, `${path}.params`).map((item, index) => {
    const paramRecord = expectRecord(item, `${path}.params[${index}]`);
    return {
      name: expectString(paramRecord.name, `${path}.params[${index}].name`),
      c: expectString(paramRecord.c, `${path}.params[${index}].c`),
    };
  });

  const conventionRaw = record.convention;
  let convention: "cdecl" | "stdcall" | undefined;
  if (conventionRaw !== undefined && conventionRaw !== null) {
    const value = expectString(conventionRaw, `${path}.convention`);
    if (value !== "cdecl" && value !== "stdcall") {
      throw new AbiParseError(`${path}.convention must be "cdecl" or "stdcall"`);
    }
    convention = value;
  }

  return convention === undefined
    ? {
        returnCType: expectString(record.returnCType, `${path}.returnCType`),
        params,
      }
    : {
        convention,
        returnCType: expectString(record.returnCType, `${path}.returnCType`),
        params,
      };
}

function parseCallbackBinding(input: unknown, path: string): AbiCallbackBinding {
  const record = expectRecord(input, path);
  const lifetime = expectString(record.lifetime, `${path}.lifetime`);
  if (lifetime !== "transient" && lifetime !== "registered") {
    throw new AbiParseError(`${path}.lifetime must be "transient" or "registered"`);
  }

  return {
    callbackParam: expectString(record.callbackParam, `${path}.callbackParam`),
    contextParam: optionalString(record.contextParam, `${path}.contextParam`),
    bindingId: expectString(record.bindingId, `${path}.bindingId`),
    obligation: expectString(record.obligation, `${path}.obligation`),
    signature: parseCallbackType(record.signature, `${path}.signature`),
    lifetime,
  };
}

function parseParam(input: unknown, path: string): AbiParam {
  const record = expectRecord(input, path);
  return {
    name: expectString(record.name, `${path}.name`),
    fzy: expectString(record.fzy, `${path}.fzy`),
    c: expectString(record.c, `${path}.c`),
    contract: parseParamContract(record.contract, `${path}.contract`),
  };
}

function parseReturn(input: unknown, path: string): AbiReturn {
  const record = expectRecord(input, path);
  return {
    fzy: expectString(record.fzy, `${path}.fzy`),
    c: expectString(record.c, `${path}.c`),
    contract: parseReturnContract(record.contract, `${path}.contract`),
  };
}

function parseExportContract(input: unknown, path: string): AbiExportContract {
  const record = expectRecord(input, path);
  const execution = expectString(record.execution, `${path}.execution`);
  if (execution !== "sync" && execution !== "async-handle-v1") {
    throw new AbiParseError(`${path}.execution is unsupported`);
  }
  const callbackBindings = expectArray(record.callbackBindings, `${path}.callbackBindings`).map(
    (binding, index) => parseCallbackBinding(binding, `${path}.callbackBindings[${index}]`),
  );

  let asyncBoundary = null;
  if (record.asyncBoundary !== null && record.asyncBoundary !== undefined) {
    const asyncRecord = expectRecord(record.asyncBoundary, `${path}.asyncBoundary`);
    const model = expectString(asyncRecord.model, `${path}.asyncBoundary.model`);
    if (model !== "async-handle-v1") {
      throw new AbiParseError(`${path}.asyncBoundary.model is unsupported`);
    }
    asyncBoundary = {
      model,
      startSymbol: expectString(asyncRecord.startSymbol, `${path}.asyncBoundary.startSymbol`),
      pollSymbol: expectString(asyncRecord.pollSymbol, `${path}.asyncBoundary.pollSymbol`),
      awaitSymbol: expectString(asyncRecord.awaitSymbol, `${path}.asyncBoundary.awaitSymbol`),
      dropSymbol: expectString(asyncRecord.dropSymbol, `${path}.asyncBoundary.dropSymbol`),
      resultType: expectString(asyncRecord.resultType, `${path}.asyncBoundary.resultType`),
    } as const;
  }

  return {
    execution,
    callbackBindings,
    asyncBoundary,
  };
}

function parseExport(input: unknown, path: string): AbiExport {
  const record = expectRecord(input, path);
  return {
    name: expectString(record.name, `${path}.name`),
    async: expectBoolean(record.async, `${path}.async`),
    symbolVersion: expectInteger(record.symbolVersion, `${path}.symbolVersion`),
    params: expectArray(record.params, `${path}.params`).map((item, index) =>
      parseParam(item, `${path}.params[${index}]`),
    ),
    return: parseReturn(record.return, `${path}.return`),
    contract: parseExportContract(record.contract, `${path}.contract`),
  };
}

function parseField(input: unknown, path: string): AbiField {
  const record = expectRecord(input, path);
  return {
    name: expectString(record.name, `${path}.name`),
    c: expectString(record.c, `${path}.c`),
  };
}

function parseLayout(input: unknown, path: string): AbiReprCLayout {
  const record = expectRecord(input, path);
  const kind = expectString(record.kind, `${path}.kind`);
  if (kind !== "struct" && kind !== "enum") {
    throw new AbiParseError(`${path}.kind must be "struct" or "enum"`);
  }

  const fields = record.fields === undefined ? undefined : expectArray(record.fields, `${path}.fields`).map(
    (field, index) => parseField(field, `${path}.fields[${index}]`),
  );
  const variants =
    record.variants === undefined
      ? undefined
      : expectArray(record.variants, `${path}.variants`).map((variant, index) => {
          const variantRecord = expectRecord(variant, `${path}.variants[${index}]`);
          const rawValue = variantRecord.value;
          let value: number | bigint;
          if (typeof rawValue === "bigint") {
            value = rawValue;
          } else if (typeof rawValue === "number" && Number.isInteger(rawValue)) {
            value = rawValue;
          } else if (typeof rawValue === "string" && /^-?\d+$/.test(rawValue)) {
            value = BigInt(rawValue);
          } else {
            throw new AbiParseError(`${path}.variants[${index}].value must be an integer`);
          }
          return {
            name: expectString(variantRecord.name, `${path}.variants[${index}].name`),
            value,
          };
        });

  const parsed: AbiReprCLayout = {
    name: expectString(record.name, `${path}.name`),
    kind,
    size: expectInteger(record.size, `${path}.size`),
    align: expectInteger(record.align, `${path}.align`),
  };
  if (fields !== undefined) {
    parsed.fields = fields;
  }
  if (variants !== undefined) {
    parsed.variants = variants;
  }
  if (record.storage !== undefined) {
    parsed.storage = expectString(record.storage, `${path}.storage`);
  }
  return parsed;
}

export function parseAbiManifest(input: string | unknown): FozzyAbiManifest {
  const parsed = typeof input === "string" ? (JSON.parse(input) as unknown) : input;
  const record = expectRecord(parsed, "manifest");
  const packageRecord = expectRecord(record.package, "manifest.package");
  const layoutPolicy = expectRecord(record.layoutPolicy, "manifest.layoutPolicy");

  const manifest: FozzyAbiManifest = {
    schemaVersion: expectString(record.schemaVersion, "manifest.schemaVersion") as FozzyAbiManifest["schemaVersion"],
    package: {
      name: expectString(packageRecord.name, "manifest.package.name"),
      version: expectString(packageRecord.version, "manifest.package.version"),
    },
    abiRevision: expectInteger(record.abiRevision, "manifest.abiRevision"),
    targetTriple: expectString(record.targetTriple, "manifest.targetTriple"),
    dataLayoutHash: expectString(record.dataLayoutHash, "manifest.dataLayoutHash"),
    compilerIdentityHash: expectString(record.compilerIdentityHash, "manifest.compilerIdentityHash"),
    panicBoundary: expectPanicBoundary(record.panicBoundary, "manifest.panicBoundary"),
    layoutPolicy: {
      reprCStableOnly: expectBoolean(
        layoutPolicy.reprCStableOnly,
        "manifest.layoutPolicy.reprCStableOnly",
      ),
      nonReprCUnstable: expectBoolean(
        layoutPolicy.nonReprCUnstable,
        "manifest.layoutPolicy.nonReprCUnstable",
      ),
    },
    symbolVersioning: expectString(record.symbolVersioning, "manifest.symbolVersioning"),
    contractSchema: expectString(record.contractSchema, "manifest.contractSchema"),
    reprCLayouts: expectArray(record.reprCLayouts, "manifest.reprCLayouts").map((item, index) =>
      parseLayout(item, `manifest.reprCLayouts[${index}]`),
    ),
    exports: expectArray(record.exports, "manifest.exports").map((item, index) =>
      parseExport(item, `manifest.exports[${index}]`),
    ),
  };

  validateAbiManifest(manifest);
  return manifest;
}

export function validateAbiManifest(manifest: FozzyAbiManifest): void {
  if (manifest.schemaVersion !== "fozzylang.ffi_abi.v1") {
    throw new AbiValidationError(`unsupported schemaVersion: ${manifest.schemaVersion}`);
  }

  const layoutNames = new Set(manifest.reprCLayouts.map((layout) => layout.name));
  const exportNames = new Set<string>();

  for (const layout of manifest.reprCLayouts) {
    if (layout.kind === "struct" && (!layout.fields || layout.fields.length === 0)) {
      throw new AbiValidationError(`repr(C) struct layout ${layout.name} must include fields`);
    }
    if (layout.kind === "enum" && (!layout.variants || layout.variants.length === 0)) {
      throw new AbiValidationError(`repr(C) enum layout ${layout.name} must include variants`);
    }
  }

  for (const abiExport of manifest.exports) {
    if (exportNames.has(abiExport.name)) {
      throw new AbiValidationError(`duplicate export name: ${abiExport.name}`);
    }
    exportNames.add(abiExport.name);

    if (abiExport.async !== (abiExport.contract.execution === "async-handle-v1")) {
      throw new AbiValidationError(
        `export ${abiExport.name} async flag does not match execution contract`,
      );
    }
    if (abiExport.async && abiExport.contract.asyncBoundary === null) {
      throw new AbiValidationError(`async export ${abiExport.name} is missing asyncBoundary`);
    }
    if (!abiExport.async && abiExport.contract.asyncBoundary !== null) {
      throw new AbiValidationError(`sync export ${abiExport.name} must not declare asyncBoundary`);
    }
    if (abiExport.async && abiExport.contract.asyncBoundary?.resultType !== abiExport.return.c) {
      throw new AbiValidationError(
        `async export ${abiExport.name} asyncBoundary.resultType must match return type`,
      );
    }

    const paramNames = new Set<string>();
    for (const param of abiExport.params) {
      if (paramNames.has(param.name)) {
        throw new AbiValidationError(`duplicate param ${param.name} on export ${abiExport.name}`);
      }
      paramNames.add(param.name);
      validateCTypeReferences(param.c, layoutNames, `export ${abiExport.name} param ${param.name}`);
    }

    validateCTypeReferences(
      abiExport.return.c,
      layoutNames,
      `export ${abiExport.name} return`,
    );

    for (const binding of abiExport.contract.callbackBindings) {
      const callbackParam = abiExport.params.find((param) => param.name === binding.callbackParam);
      if (!callbackParam) {
        throw new AbiValidationError(
          `callback binding ${binding.bindingId} references missing param ${binding.callbackParam}`,
        );
      }
      if (!callbackParam.c.includes("*")) {
        throw new AbiValidationError(
          `callback binding ${binding.bindingId} callback param ${binding.callbackParam} must be pointer-like`,
        );
      }
      if (binding.contextParam === binding.callbackParam) {
        throw new AbiValidationError(
          `callback binding ${binding.bindingId} context param must differ from callback param`,
        );
      }
      if (callbackParam.contract.nullability !== "non_null") {
        throw new AbiValidationError(
          `callback binding ${binding.bindingId} callback param ${binding.callbackParam} must be non_null`,
        );
      }
      const contextParam =
        binding.contextParam === null
          ? null
          : abiExport.params.find((param) => param.name === binding.contextParam) ?? null;
      if (binding.contextParam !== null && contextParam === null) {
        throw new AbiValidationError(
          `callback binding ${binding.bindingId} references missing context param ${binding.contextParam}`,
        );
      }
      if (contextParam !== null && !contextParam.c.includes("*")) {
        throw new AbiValidationError(
          `callback binding ${binding.bindingId} context param ${binding.contextParam} must be pointer-like`,
        );
      }
      validateCTypeReferences(
        binding.signature.returnCType,
        layoutNames,
        `callback binding ${binding.bindingId} return`,
      );
      for (const param of binding.signature.params) {
        validateCTypeReferences(
          param.c,
          layoutNames,
          `callback binding ${binding.bindingId} param ${param.name}`,
        );
      }
      if (contextParam === null) {
        if (binding.signature.params.length > 0 && last(binding.signature.params)?.c === "void*") {
          throw new AbiValidationError(
            `callback binding ${binding.bindingId} callback signature includes context-like void* param without contextParam`,
          );
        }
      } else {
        const signatureContextParam = last(binding.signature.params);
        if (!signatureContextParam) {
          throw new AbiValidationError(
            `callback binding ${binding.bindingId} must include a context signature param for ${binding.contextParam}`,
          );
        }
        if (normalizeBaseCType(signatureContextParam.c) !== normalizeBaseCType(contextParam.c)) {
          throw new AbiValidationError(
            `callback binding ${binding.bindingId} context signature type must match context param ${binding.contextParam}`,
          );
        }
      }
    }
  }
}

function last<T>(values: readonly T[]): T | undefined {
  return values.length === 0 ? undefined : values[values.length - 1];
}

function validateCTypeReferences(cType: string, layoutNames: Set<string>, path: string): void {
  const normalized = normalizeBaseCType(cType);
  if (isBuiltinCType(normalized)) {
    return;
  }
  if (layoutNames.has(normalized)) {
    return;
  }
  if (normalized === "fz_async_handle_t") {
    return;
  }
  throw new AbiValidationError(`${path} references unknown C type ${normalized}`);
}

function normalizeBaseCType(cType: string): string {
  return cType.replace(/\bconst\b/g, "").replace(/\*/g, "").trim().replace(/\s+/g, " ");
}

function isBuiltinCType(cType: string): boolean {
  return new Set([
    "void",
    "bool",
    "size_t",
    "ssize_t",
    "char",
    "unsigned char",
    "signed char",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "__uint128_t",
    "__int128_t",
    "float",
    "double",
    "uint32",
    "int32",
    "uint64",
    "int64",
  ]).has(cType);
}
