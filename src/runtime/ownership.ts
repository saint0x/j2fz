import koffi from "koffi";
import type { LibraryHandle } from "koffi";

import type { AbiExport, AbiParam } from "../types/abi.js";
import { OwnershipError, TypeMarshalingError } from "./errors.js";
import { decodeOwnedString, koffiTypeFromCType, type RuntimeTypeRegistry } from "./koffi.js";

export interface OwnedPointerReleaserMap {
  readonly [exportName: string]: string;
}

export interface NativeOwnedPointer<TBrand extends string = string> {
  readonly __brand: TBrand;
  readonly exportName: string;
  readonly cType: string;
  readonly pointer: unknown;
  readonly disposed: boolean;
  decodeBytes(length: number): Uint8Array;
  decodeString(): string;
  dispose(): void;
}

export function adaptCallArgs(
  abiExport: AbiExport,
  args: unknown[],
): unknown[] {
  if (args.length !== abiExport.params.length) {
    throw new TypeMarshalingError(
      `export ${abiExport.name} expected ${abiExport.params.length} args but got ${args.length}`,
    );
  }

  return abiExport.params.map((param, index) => adaptParamArg(abiExport, param, args[index]));
}

export function adaptResultValue(
  abiExport: AbiExport,
  result: unknown,
  library: LibraryHandle,
  registry: RuntimeTypeRegistry,
  releasers: OwnedPointerReleaserMap | undefined,
): unknown {
  const ownership = abiExport.return.contract.ownership;
  if (ownership !== "owned") {
    return result;
  }

  if (!abiExport.return.c.includes("*")) {
    return result;
  }

  const releaserSymbol = releasers?.[abiExport.name];
  const baseCType = abiExport.return.c.replace(/\bconst\b/g, "").replace(/\*/g, "").trim().replace(/\s+/g, " ");

  let disposed = false;
  const release =
    releaserSymbol === undefined
      ? null
      : library.func(releaserSymbol, "void", [koffiTypeFromCType(abiExport.return.c, registry.layouts)]);

  const handle: NativeOwnedPointer = {
    __brand: baseCType,
    exportName: abiExport.name,
    cType: abiExport.return.c,
    pointer: result,
    get disposed() {
      return disposed;
    },
    decodeBytes(length: number) {
      if (disposed) {
        throw new OwnershipError(`owned pointer from ${abiExport.name} is already disposed`);
      }
      return decodeOwnedBytes(result, length);
    },
    decodeString() {
      if (disposed) {
        throw new OwnershipError(`owned pointer from ${abiExport.name} is already disposed`);
      }
      if (!isStringLikePointerType(abiExport.return.c)) {
        throw new OwnershipError(
          `owned pointer from ${abiExport.name} is not string-like; use decodeBytes(length) instead`,
        );
      }
      return decodeOwnedString(result);
    },
    dispose() {
      if (disposed) {
        return;
      }
      disposed = true;
      if (release !== null) {
        release(result);
      }
    },
  };
  return handle;
}

function adaptParamArg(
  abiExport: AbiExport,
  param: AbiParam,
  value: unknown,
): unknown {
  const cType = param.c.replace(/\s+/g, " ").trim();
  const ownership = param.contract.ownership;
  const view = param.contract.view;

  if (cType === "size_t" || cType === "uint64_t" || cType === "int64_t" || cType === "fz_async_handle_t") {
    return coerceWideInteger(abiExport.name, param.name, value);
  }

  if (cType === "uint32_t" || cType === "int32_t" || cType === "uint16_t" || cType === "int16_t" || cType === "uint8_t" || cType === "int8_t") {
    return coerceNumber(abiExport.name, param.name, value);
  }

  if (cType === "bool") {
    if (typeof value !== "boolean") {
      throw new TypeMarshalingError(`export ${abiExport.name} param ${param.name} must be boolean`);
    }
    return value;
  }

  if (cType.includes("*") && view?.kind === "ptr_len") {
    if (ownership === "borrowed" || ownership === "inout" || ownership === "out") {
      return coerceBinaryBuffer(abiExport.name, param.name, value);
    }
  }

  if (cType.includes("*") && typeof value === "object" && value !== null && "pointer" in value) {
    return (value as { pointer: unknown }).pointer;
  }

  return value;
}

function coerceWideInteger(exportName: string, paramName: string, value: unknown): bigint {
  if (typeof value === "bigint") {
    return value;
  }
  if (typeof value === "number" && Number.isInteger(value)) {
    return BigInt(value);
  }
  throw new TypeMarshalingError(`export ${exportName} param ${paramName} must be bigint-compatible`);
}

function coerceNumber(exportName: string, paramName: string, value: unknown): number {
  if (typeof value === "number" && Number.isFinite(value)) {
    return value;
  }
  throw new TypeMarshalingError(`export ${exportName} param ${paramName} must be a finite number`);
}

function coerceBinaryBuffer(exportName: string, paramName: string, value: unknown): Uint8Array | Buffer {
  if (typeof value === "string") {
    return Buffer.from(value, "utf8");
  }
  if (value instanceof Uint8Array || value instanceof Buffer) {
    return value;
  }
  throw new TypeMarshalingError(
    `export ${exportName} param ${paramName} must be Uint8Array, Buffer, or string`,
  );
}

function decodeOwnedBytes(pointer: unknown, length: number): Uint8Array {
  if (!Number.isInteger(length) || length < 0) {
    throw new TypeMarshalingError("decodeBytes(length) requires a non-negative integer length");
  }
  const arrayBuffer = koffi.view(pointer, length);
  return new Uint8Array(arrayBuffer.slice(0));
}

function isStringLikePointerType(cType: string): boolean {
  const normalized = cType.replace(/\s+/g, " ").trim();
  return normalized === "char*" || normalized === "const char*";
}
