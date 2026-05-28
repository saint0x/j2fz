import { accessSync, constants, readFileSync } from "node:fs";

import koffi, { type LibraryHandle } from "koffi";

import type { FozzyAbiManifest } from "../types/abi.js";
import type { LoadModuleOptions } from "../types/public.js";
import { parseAbiManifest } from "./manifest.js";
import { buildRuntimeTypeRegistry, type RuntimeTypeRegistry } from "./koffi.js";
import { SymbolLoadError, TypeMarshalingError } from "./errors.js";

export interface UnsafeRawPointer<TBrand extends string = string> {
  readonly __unsafe_raw_pointer: true;
  readonly __brand: TBrand;
  readonly value: unknown;
}

export interface UnsafeRawFunctionSpec {
  readonly symbol: string;
  readonly result: string;
  readonly params: readonly string[];
}

export interface UnsafeRawFunction {
  readonly spec: UnsafeRawFunctionSpec;
  call(...args: unknown[]): unknown;
}

export interface UnsafeRawModule {
  readonly manifest: FozzyAbiManifest;
  readonly library: LibraryHandle;
  readonly registry: RuntimeTypeRegistry;
  bindFunction(spec: UnsafeRawFunctionSpec): UnsafeRawFunction;
  unsafePointer<TBrand extends string>(brand: TBrand, value: unknown): UnsafeRawPointer<TBrand>;
  dispose(): void;
}

export function loadUnsafeRawModule(options: LoadModuleOptions): UnsafeRawModule {
  ensureReadable(options.paths.abiManifest);
  ensureReadable(options.paths.sharedLibrary);

  const manifest = parseAbiManifest(readFileSync(options.paths.abiManifest, "utf8"));
  if (options.package) {
    if (manifest.package.name !== options.package.name) {
      throw new SymbolLoadError(
        `manifest package name mismatch: expected ${options.package.name} got ${manifest.package.name}`,
      );
    }
    if (manifest.package.version !== options.package.version) {
      throw new SymbolLoadError(
        `manifest package version mismatch: expected ${options.package.version} got ${manifest.package.version}`,
      );
    }
  }

  const library = koffi.load(options.paths.sharedLibrary);
  const registry = buildRuntimeTypeRegistry(manifest);

  return {
    manifest,
    library,
    registry,
    bindFunction(spec: UnsafeRawFunctionSpec): UnsafeRawFunction {
      try {
        const fn = library.func(`${spec.result} ${spec.symbol}(${spec.params.join(", ")})`);
        return {
          spec,
          call(...args: unknown[]) {
            return fn(...args.map(unwrapUnsafeRawPointer));
          },
        };
      } catch (error) {
        throw new SymbolLoadError(`failed binding raw symbol ${spec.symbol}`, { cause: error });
      }
    },
    unsafePointer<TBrand extends string>(brand: TBrand, value: unknown): UnsafeRawPointer<TBrand> {
      return {
        __unsafe_raw_pointer: true,
        __brand: brand,
        value,
      };
    },
    dispose() {
      library.unload();
    },
  };
}

function ensureReadable(path: string): void {
  accessSync(path, constants.R_OK);
}

function unwrapUnsafeRawPointer(value: unknown): unknown {
  if (typeof value === "object" && value !== null && "__unsafe_raw_pointer" in value) {
    const pointer = value as Partial<UnsafeRawPointer>;
    if (pointer.__unsafe_raw_pointer !== true || !("value" in pointer)) {
      throw new TypeMarshalingError("invalid unsafe raw pointer wrapper");
    }
    return pointer.value;
  }
  return value;
}
