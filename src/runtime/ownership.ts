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

export type CallArgAdapter = (args: unknown[]) => unknown[];
export type ResultValueAdapter = (result: unknown) => unknown;

export interface CompiledOwnedPointerAdapterOptions {
  readonly release: ((pointer: unknown) => void) | null;
}

export function compileCallArgAdapter(abiExport: AbiExport): CallArgAdapter {
  const paramAdapters = abiExport.params.map((param) => compileParamAdapter(abiExport, param));
  return (args: unknown[]) => {
    if (args.length !== paramAdapters.length) {
      throw new TypeMarshalingError(
        `export ${abiExport.name} expected ${paramAdapters.length} args but got ${args.length}`,
      );
    }

    const adaptedArgs = new Array<unknown>(paramAdapters.length);
    for (let index = 0; index < paramAdapters.length; index += 1) {
      adaptedArgs[index] = paramAdapters[index]?.(args[index]);
    }
    return adaptedArgs;
  };
}

export function compileResultValueAdapter(
  abiExport: AbiExport,
  library: LibraryHandle,
  registry: RuntimeTypeRegistry,
  releasers: OwnedPointerReleaserMap | undefined,
): ResultValueAdapter {
  const ownership = abiExport.return.contract.ownership;
  if (ownership !== "owned") {
    return (result: unknown) => result;
  }

  if (!abiExport.return.c.includes("*")) {
    return (result: unknown) => result;
  }

  const releaserSymbol = releasers?.[abiExport.name];
  const baseCType = abiExport.return.c.replace(/\bconst\b/g, "").replace(/\*/g, "").trim().replace(/\s+/g, " ");
  const release =
    releaserSymbol === undefined
      ? null
      : library.func(releaserSymbol, "void", [koffiTypeFromCType(abiExport.return.c, registry.layouts)]);

  return (result: unknown) =>
    createOwnedPointerHandle(result, {
      exportName: abiExport.name,
      cType: abiExport.return.c,
      baseCType,
      supportsStringDecode: isStringLikePointerType(abiExport.return.c),
      release,
    });
}

export function adaptCallArgs(
  abiExport: AbiExport,
  args: unknown[],
): unknown[] {
  return compileCallArgAdapter(abiExport)(args);
}

export function adaptResultValue(
  abiExport: AbiExport,
  result: unknown,
  library: LibraryHandle,
  registry: RuntimeTypeRegistry,
  releasers: OwnedPointerReleaserMap | undefined,
): unknown {
  return compileResultValueAdapter(abiExport, library, registry, releasers)(result);
}

function createOwnedPointerHandle(
  result: unknown,
  options: {
    exportName: string;
    cType: string;
    baseCType: string;
    supportsStringDecode: boolean;
    release: ((pointer: unknown) => void) | null;
  },
): NativeOwnedPointer {
  let disposed = false;
  const handle: NativeOwnedPointer = {
    __brand: options.baseCType,
    exportName: options.exportName,
    cType: options.cType,
    pointer: result,
    get disposed() {
      return disposed;
    },
    decodeBytes(length: number) {
      if (disposed) {
        throw new OwnershipError(`owned pointer from ${options.exportName} is already disposed`);
      }
      return decodeOwnedBytes(result, length);
    },
    decodeString() {
      if (disposed) {
        throw new OwnershipError(`owned pointer from ${options.exportName} is already disposed`);
      }
      if (!options.supportsStringDecode) {
        throw new OwnershipError(
          `owned pointer from ${options.exportName} is not string-like; use decodeBytes(length) instead`,
        );
      }
      return decodeOwnedString(result);
    },
    dispose() {
      if (disposed) {
        return;
      }
      disposed = true;
      options.release?.(result);
    },
  };
  return handle;
}

function compileParamAdapter(
  abiExport: AbiExport,
  param: AbiParam,
): (value: unknown) => unknown {
  const cType = param.c.replace(/\s+/g, " ").trim();
  const ownership = param.contract.ownership;
  const view = param.contract.view;

  if (cType === "size_t" || cType === "uint64_t" || cType === "int64_t" || cType === "fz_async_handle_t") {
    return (value: unknown) => coerceWideInteger(abiExport.name, param.name, value);
  }

  if (cType === "uint32_t" || cType === "int32_t" || cType === "uint16_t" || cType === "int16_t" || cType === "uint8_t" || cType === "int8_t") {
    return (value: unknown) => coerceNumber(abiExport.name, param.name, value);
  }

  if (cType === "bool") {
    return (value: unknown) => {
      if (typeof value !== "boolean") {
        throw new TypeMarshalingError(`export ${abiExport.name} param ${param.name} must be boolean`);
      }
      return value;
    };
  }

  if (cType.includes("*") && view?.kind === "ptr_len") {
    if (ownership === "borrowed" || ownership === "inout" || ownership === "out") {
      return (value: unknown) => coerceBinaryBuffer(abiExport.name, param.name, value);
    }
  }

  if (cType.includes("*")) {
    return (value: unknown) => {
      if (typeof value === "object" && value !== null && "pointer" in value) {
        return (value as { pointer: unknown }).pointer;
      }
      return value;
    };
  }

  return (value: unknown) => value;
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
