import type { AbiExport, FozzyAbiManifest } from "../types/abi.js";

export interface GeneratedModuleModel {
  readonly packageName: string;
  readonly exportName: string;
  readonly exports: GeneratedExportModel[];
}

export interface GeneratedExportModel {
  readonly abiName: string;
  readonly jsName: string;
  readonly tsReturnType: string;
  readonly tsParams: GeneratedParamModel[];
  readonly isAsync: boolean;
}

export interface GeneratedParamModel {
  readonly abiName: string;
  readonly jsName: string;
  readonly tsType: string;
}

export function buildGeneratedModuleModel(
  manifest: FozzyAbiManifest,
  exportName = "createBindings",
): GeneratedModuleModel {
  return {
    packageName: manifest.package.name,
    exportName,
    exports: manifest.exports.map((abiExport) => buildGeneratedExportModel(abiExport)),
  };
}

function buildGeneratedExportModel(abiExport: AbiExport): GeneratedExportModel {
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
      return ownership === "owned" ? "string | bigint" : "string | Uint8Array | Buffer";
    }
    if (base === "uint8_t" || base === "int8_t") {
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
