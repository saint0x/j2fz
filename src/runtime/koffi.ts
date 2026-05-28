import koffi, { type LibraryHandle, type TypeObject } from "koffi";

import type {
  AbiCallbackBinding,
  AbiExport,
  AbiField,
  AbiParam,
  AbiReprCLayout,
  FozzyAbiManifest,
} from "../types/abi.js";
import { AbiValidationError, CallbackError, TypeMarshalingError } from "./errors.js";

export interface RuntimeTypeRegistry {
  readonly layouts: Map<string, TypeObject>;
}

export type KoffiTypeSpec = Parameters<typeof koffi.type>[0];

export interface RuntimeFunctionDeclaration {
  readonly symbol: string;
  readonly result: KoffiTypeSpec;
  readonly params: KoffiTypeSpec[];
}

export interface RegisteredCallbackHandle {
  readonly binding: AbiCallbackBinding;
  readonly pointer: bigint;
  dispose(): void;
}

const DIRECT_C_TYPE_MAP = new Map<string, KoffiTypeSpec>([
  ["void", "void"],
  ["bool", "bool"],
  ["char", "char"],
  ["uint8_t", "uint8_t"],
  ["uint16_t", "uint16_t"],
  ["uint32_t", "uint32_t"],
  ["uint64_t", "uint64_t"],
  ["int8_t", "int8_t"],
  ["int16_t", "int16_t"],
  ["int32_t", "int32_t"],
  ["int64_t", "int64_t"],
  ["size_t", "size_t"],
  ["ssize_t", "intptr_t"],
  ["float", "float"],
  ["double", "double"],
  ["fz_async_handle_t", "uint64_t"],
]);

export function buildRuntimeTypeRegistry(manifest: FozzyAbiManifest): RuntimeTypeRegistry {
  const layouts = new Map<string, TypeObject>();
  for (const layout of manifest.reprCLayouts) {
    layouts.set(layout.name, buildLayoutType(layout, layouts));
  }
  return { layouts };
}

function buildLayoutType(layout: AbiReprCLayout, knownLayouts: Map<string, TypeObject>): TypeObject {
  if (layout.kind === "struct") {
    const fields = Object.fromEntries(
      (layout.fields ?? []).map((field) => [field.name, koffiTypeForField(field, knownLayouts)]),
    );
    return koffi.struct(layout.name, fields);
  }

  const variants = Object.fromEntries(
    (layout.variants ?? []).map((variant) => [variant.name, variant.value]),
  );
  return koffi.enumeration(layout.name, variants, layout.storage ?? "int32_t");
}

function koffiTypeForField(field: AbiField, knownLayouts: Map<string, TypeObject>): KoffiTypeSpec {
  return koffiTypeFromCType(field.c, knownLayouts);
}

export function declareKoffiFunction(
  abiExport: AbiExport,
  registry: RuntimeTypeRegistry,
): RuntimeFunctionDeclaration {
  return {
    symbol: abiExport.name,
    result: koffiTypeFromCType(abiExport.return.c, registry.layouts),
    params: abiExport.params.map((param) => koffiTypeForParam(param, abiExport, registry)),
  };
}

export function bindKoffiFunction(
  library: LibraryHandle,
  abiExport: AbiExport,
  registry: RuntimeTypeRegistry,
): (...args: unknown[]) => unknown {
  const decl = declareKoffiFunction(abiExport, registry);
  const fn = library.func(decl.symbol, decl.result, decl.params);
  return (...args: unknown[]) => fn(...args);
}

export function koffiTypeForParam(
  param: AbiParam,
  abiExport: AbiExport,
  registry: RuntimeTypeRegistry,
): KoffiTypeSpec {
  const callbackBinding = abiExport.contract.callbackBindings.find(
    (binding) => binding.callbackParam === param.name,
  );
  if (callbackBinding) {
    return pointerWrappedCallbackType(callbackBinding, registry.layouts);
  }

  const baseType = koffiTypeFromCType(param.c, registry.layouts);
  const ownership = param.contract.ownership;
  if (ownership === "out") {
    return koffi.out(baseType);
  }
  if (ownership === "inout") {
    return koffi.inout(baseType);
  }
  return baseType;
}

export function pointerWrappedCallbackType(
  binding: AbiCallbackBinding,
  knownLayouts: Map<string, TypeObject>,
): TypeObject {
  return koffi.pointer(callbackPrototypeType(binding, knownLayouts));
}

export function callbackPrototypeType(
  binding: AbiCallbackBinding,
  knownLayouts: Map<string, TypeObject>,
): TypeObject {
  const callbackParams = binding.signature.params.map((param) => koffiTypeFromCType(param.c, knownLayouts));
  const callbackResult = koffiTypeFromCType(binding.signature.returnCType, knownLayouts);
  return binding.signature.convention === "stdcall"
    ? koffi.proto("stdcall", null, callbackResult, callbackParams)
    : koffi.proto(null, callbackResult, callbackParams);
}

export function registerCallback(
  binding: AbiCallbackBinding,
  fn: (...args: unknown[]) => unknown,
  registry: RuntimeTypeRegistry,
): RegisteredCallbackHandle {
  try {
    const type = pointerWrappedCallbackType(binding, registry.layouts);
    const pointer =
      binding.lifetime === "registered"
        ? koffi.register(fn, type)
        : koffi.register(fn, type);
    let active = true;
    return {
      binding,
      pointer,
      dispose() {
        if (!active) {
          return;
        }
        active = false;
        koffi.unregister(pointer);
      },
    };
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    throw new CallbackError(`failed to register callback ${binding.bindingId}: ${detail}`, { cause: error });
  }
}

export function encodeStringArgument(value: string): string {
  return value;
}

export function decodeOwnedString(pointer: unknown): string {
  try {
    return koffi.decode(pointer, "str") as string;
  } catch (error) {
    throw new TypeMarshalingError("failed to decode owned string", { cause: error });
  }
}

export function koffiTypeFromCType(
  cType: string,
  knownLayouts: Map<string, TypeObject>,
): KoffiTypeSpec {
  const parsed = parseCType(cType);

  if (parsed.pointerDepth === 0) {
    const direct = DIRECT_C_TYPE_MAP.get(parsed.base);
    if (direct !== undefined) {
      return direct;
    }
    const named = knownLayouts.get(parsed.base);
    if (named !== undefined) {
      return named;
    }
    throw new AbiValidationError(`unsupported C type ${cType}`);
  }

  if (parsed.base === "char" && parsed.pointerDepth === 1 && parsed.isConst) {
    return "str";
  }

  let inner: KoffiTypeSpec;
  if (parsed.base === "void") {
    inner = koffi.opaque();
  } else if (DIRECT_C_TYPE_MAP.has(parsed.base)) {
    inner = DIRECT_C_TYPE_MAP.get(parsed.base) as KoffiTypeSpec;
  } else {
    const named = knownLayouts.get(parsed.base);
    if (named === undefined) {
      throw new AbiValidationError(`unsupported pointer base type ${cType}`);
    }
    inner = named;
  }

  for (let depth = 0; depth < parsed.pointerDepth; depth += 1) {
    inner = koffi.pointer(inner);
  }
  return inner;
}

interface ParsedCType {
  base: string;
  isConst: boolean;
  pointerDepth: number;
}

function parseCType(raw: string): ParsedCType {
  const pointerDepth = (raw.match(/\*/g) ?? []).length;
  const withoutPointers = raw.replace(/\*/g, " ").trim().replace(/\s+/g, " ");
  const isConst = /\bconst\b/.test(withoutPointers);
  const base = withoutPointers.replace(/\bconst\b/g, "").trim().replace(/\s+/g, " ");
  return {
    base,
    isConst,
    pointerDepth,
  };
}

export function assertSupportedExportSubset(abiExport: AbiExport): void {
  if (
    abiExport.contract.callbackBindings.length > 0 &&
    abiExport.params.every(
      (param) => !abiExport.contract.callbackBindings.some((binding) => binding.callbackParam === param.name),
    )
  ) {
    throw new AbiValidationError(`callback binding metadata is inconsistent on export ${abiExport.name}`);
  }
}
