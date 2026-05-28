import type { AbiCallbackBinding, AbiExport, FozzyAbiManifest } from "../types/abi.js";

export interface GeneratedModuleModel {
  readonly packageName: string;
  readonly exportName: string;
  readonly exports: GeneratedExportModel[];
  readonly handleTypes: GeneratedHandleTypeModel[];
  readonly callbacks: GeneratedCallbackModel[];
}

export interface GeneratedExportModel {
  readonly abiName: string;
  readonly jsName: string;
  readonly tsReturnType: string;
  readonly tsParams: GeneratedParamModel[];
  readonly isAsync: boolean;
  readonly returnsOwnedHandle: boolean;
}

export interface GeneratedParamModel {
  readonly abiName: string;
  readonly jsName: string;
  readonly tsType: string;
}

export interface GeneratedHandleTypeModel {
  readonly typeName: string;
  readonly brand: string;
  readonly cType: string;
  readonly supportsStringDecode: boolean;
}

export interface GeneratedCallbackModel {
  readonly exportAbiName: string;
  readonly methodName: string;
  readonly bindingId: string;
  readonly callbackType: string;
}

export function buildGeneratedModuleModel(
  manifest: FozzyAbiManifest,
  exportName = "createBindings",
): GeneratedModuleModel {
  const handleTypes = collectHandleTypes(manifest.exports);
  return {
    packageName: manifest.package.name,
    exportName,
    exports: manifest.exports.map((abiExport) => buildGeneratedExportModel(abiExport)),
    handleTypes,
    callbacks: collectCallbacks(manifest.exports),
  };
}

function buildGeneratedExportModel(abiExport: AbiExport): GeneratedExportModel {
  const returnsOwnedHandle = returnsOwnedPointerHandle(abiExport);
  return {
    abiName: abiExport.name,
    jsName: sanitizeIdentifier(abiExport.name),
    tsReturnType: abiExport.async
      ? `Promise<${renderTsTypeForReturn(abiExport)}>`
      : renderTsTypeForReturn(abiExport),
    tsParams: abiExport.params.map((param) => ({
      abiName: param.name,
      jsName: sanitizeIdentifier(param.name),
      tsType: renderTsTypeForParam(param.c, param.contract.ownership),
    })),
    isAsync: abiExport.async,
    returnsOwnedHandle,
  };
}

function renderTsTypeForReturn(abiExport: AbiExport): string {
  return renderTsTypeForCType(abiExport.return.c, abiExport.return.contract.ownership);
}

function renderTsTypeForParam(cType: string, ownership: string): string {
  return renderTsTypeForCType(cType, ownership);
}

export function renderTsTypeForCType(cType: string, ownership: string): string {
  const normalized = cType.replace(/\bconst\b/g, "").trim().replace(/\s+/g, " ");
  if (normalized.endsWith("*")) {
    const base = normalized.replace(/\*/g, "").trim();
    if (base === "char") {
      return ownership === "owned"
        ? ownedHandleTypeName(base)
        : "string | Uint8Array | Buffer";
    }
    if (base === "uint8_t" || base === "int8_t") {
      if (ownership === "owned") {
        return ownedHandleTypeName(base);
      }
      if (ownership === "out" || ownership === "inout") {
        return "Uint8Array | Buffer";
      }
      return "Uint8Array | Buffer";
    }
    return `OpaqueHandle<${JSON.stringify(base)}>`;
  }

  switch (normalized) {
    case "bool":
      return "boolean";
    case "float":
    case "double":
    case "int8_t":
    case "int16_t":
    case "int32_t":
    case "uint8_t":
    case "uint16_t":
    case "uint32_t":
      return "number";
    case "int64_t":
    case "uint64_t":
    case "size_t":
    case "ssize_t":
    case "fz_async_handle_t":
      return "bigint";
    case "char":
      return "number";
    default:
      return sanitizeTypeReference(normalized);
  }
}

function sanitizeIdentifier(value: string): string {
  const rewritten = value.replace(/[^A-Za-z0-9_$]/g, "_");
  if (/^[0-9]/.test(rewritten)) {
    return `_${rewritten}`;
  }
  return rewritten;
}

function sanitizeTypeReference(value: string): string {
  return value.replace(/[^A-Za-z0-9_$]/g, "_");
}

function returnsOwnedPointerHandle(abiExport: AbiExport): boolean {
  return abiExport.return.contract.ownership === "owned" && abiExport.return.c.includes("*");
}

function collectHandleTypes(exports: AbiExport[]): GeneratedHandleTypeModel[] {
  const byName = new Map<string, GeneratedHandleTypeModel>();
  for (const abiExport of exports) {
    if (!returnsOwnedPointerHandle(abiExport)) {
      continue;
    }
    const base = abiExport.return.c.replace(/\bconst\b/g, "").replace(/\*/g, "").trim().replace(/\s+/g, " ");
    const typeName = ownedHandleTypeName(base);
    if (!byName.has(typeName)) {
      byName.set(typeName, {
        typeName,
        brand: base,
        cType: abiExport.return.c,
        supportsStringDecode: base === "char",
      });
    }
  }
  return [...byName.values()];
}

function ownedHandleTypeName(base: string): string {
  return `${sanitizeTypeReference(base)}OwnedHandle`;
}

function collectCallbacks(exports: AbiExport[]): GeneratedCallbackModel[] {
  const callbacks: GeneratedCallbackModel[] = [];
  for (const abiExport of exports) {
    for (const binding of abiExport.contract.callbackBindings) {
      callbacks.push({
        exportAbiName: abiExport.name,
        methodName: sanitizeIdentifier(`register_${abiExport.name}_${binding.bindingId}`),
        bindingId: binding.bindingId,
        callbackType: renderCallbackType(binding),
      });
    }
  }
  return callbacks;
}

function renderCallbackType(binding: AbiCallbackBinding): string {
  const params = binding.signature.params
    .map((param) => `${sanitizeIdentifier(param.name)}: ${renderTsTypeForCType(param.c, "value")}`)
    .join(", ");
  const returnType = renderTsTypeForCType(binding.signature.returnCType, "value");
  return `(${params}) => ${returnType}`;
}
