import { accessSync, constants, readFileSync } from "node:fs";

import koffi, { type LibraryHandle } from "koffi";

import type { AbiExport, FozzyAbiManifest } from "../types/abi.js";
import type { LoadModuleOptions } from "../types/public.js";
import {
  AsyncInteropError,
  NativeBoundaryError,
  OwnershipError,
  SymbolLoadError,
} from "./errors.js";
import { parseAbiManifest } from "./manifest.js";
import {
  assertSupportedExportSubset,
  bindKoffiFunction,
  buildRuntimeTypeRegistry,
  koffiTypeForParam,
  registerCallback,
  type RegisteredCallbackHandle,
  type RuntimeTypeRegistry,
} from "./koffi.js";
import { adaptCallArgs, adaptResultValue } from "./ownership.js";

export interface LoadedFozzyModule {
  readonly manifest: FozzyAbiManifest;
  readonly library: LibraryHandle;
  readonly registry: RuntimeTypeRegistry;
  readonly exports: ReadonlyMap<string, LoadedExport>;
  dispose(): void;
}

export interface LoadedExport {
  readonly abi: AbiExport;
  call(...args: unknown[]): unknown;
  registerCallback(bindingId: string, fn: (...args: unknown[]) => unknown): RegisteredCallbackHandle;
}

export function loadFozzyModule(options: LoadModuleOptions): LoadedFozzyModule {
  ensureReadable(options.paths.abiManifest);
  ensureReadable(options.paths.sharedLibrary);

  const manifestText = readFileSync(options.paths.abiManifest, "utf8");
  const manifest = parseAbiManifest(manifestText);
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
  const loadedExports = new Map<string, LoadedExport>();
  const activeCallbacks = new Set<RegisteredCallbackHandle>();
  const pollIntervalMs = options.pollIntervalMs ?? 5;
  const asyncTimeoutMs = options.asyncTimeoutMs ?? 5_000;
  const releasers = options.ownedPointerReleasers;

  for (const abiExport of manifest.exports) {
    assertSupportedExportSubset(abiExport);

    const rawCall = bindKoffiFunction(library, abiExport, registry);
    const loaded: LoadedExport = {
      abi: abiExport,
      call(...args: unknown[]) {
        if (abiExport.async) {
          return invokeAsyncHandleExport(
            rawCall,
            abiExport,
            manifest,
            library,
            registry,
            args,
            pollIntervalMs,
            asyncTimeoutMs,
          );
        }
        try {
          const adaptedArgs = adaptCallArgs(abiExport, args);
          const result = rawCall(...adaptedArgs);
          return adaptResultValue(abiExport, result, library, registry, releasers);
        } catch (error) {
          throw new NativeBoundaryError(`native call failed for ${abiExport.name}`, { cause: error });
        }
      },
      registerCallback(bindingId: string, fn: (...args: unknown[]) => unknown) {
        const binding = abiExport.contract.callbackBindings.find((item) => item.bindingId === bindingId);
        if (!binding) {
          throw new OwnershipError(`callback binding ${bindingId} does not exist on ${abiExport.name}`);
        }
        const handle = registerCallback(binding, fn, registry);
        activeCallbacks.add(handle);
        return {
          ...handle,
          dispose() {
            handle.dispose();
            activeCallbacks.delete(handle);
          },
        };
      },
    };
    loadedExports.set(abiExport.name, loaded);
  }

  return {
    manifest,
    library,
    registry,
    exports: loadedExports,
    dispose() {
      for (const handle of activeCallbacks) {
        handle.dispose();
      }
      activeCallbacks.clear();
      library.unload();
    },
  };
}

function ensureReadable(path: string): void {
  accessSync(path, constants.R_OK);
}

function invokeAsyncHandleExport(
  _rawCall: (...args: unknown[]) => unknown,
  abiExport: AbiExport,
  manifest: FozzyAbiManifest,
  library: LibraryHandle,
  registry: RuntimeTypeRegistry,
  args: unknown[],
  pollIntervalMs: number,
  asyncTimeoutMs: number,
): Promise<unknown> {
  const boundary = abiExport.contract.asyncBoundary;
  if (boundary === null) {
    throw new AsyncInteropError(`async export ${abiExport.name} is missing async boundary`);
  }

  try {
    const start = library.func(
      `int32_t ${boundary.startSymbol}(${[
        ...abiExport.params.map((param) => renderPrototypeParam(abiExport, param.name, registry)),
        "_Out_ uint64_t *handle_out",
      ].join(", ")})`,
    );
    const poll = library.func(`int32_t ${boundary.pollSymbol}(uint64_t handle, _Out_ int32_t *done_out)`);
    const awaitResult = library.func(
      `int32_t ${boundary.awaitSymbol}(uint64_t handle, _Out_ ${boundary.resultType} *result_out)`,
    );
    const drop = library.func(`int32_t ${boundary.dropSymbol}(uint64_t handle)`);

    const handleOut: Array<bigint | number | null> = [null];
    const startCode = start(...args, handleOut);
    if (typeof startCode !== "number" || startCode !== 0) {
      throw new AsyncInteropError(`async start failed for ${abiExport.name}: code=${String(startCode)}`);
    }
    const handle = handleOut[0];
    if (typeof handle !== "bigint" && typeof handle !== "number") {
      throw new AsyncInteropError(`async start did not return a valid handle for ${abiExport.name}`);
    }

    return new Promise((resolve, reject) => {
      let settled = false;
      let lastDoneValue: unknown = null;
      const finish = (fn: () => void) => {
        if (settled) {
          return;
        }
        settled = true;
        clearInterval(timer);
        clearTimeout(timeout);
        fn();
      };
      const timer = setInterval(() => {
        try {
          const doneOut: Array<number | null> = [null];
          const pollCode = poll(handle, doneOut);
          if (typeof pollCode !== "number" || pollCode !== 0) {
            finish(() => {
              tryDrop(drop, handle);
              reject(new AsyncInteropError(`async poll failed for ${abiExport.name}: code=${String(pollCode)}`));
            });
            return;
          }
          lastDoneValue = doneOut[0];
          if (!isCompletionSignal(doneOut[0])) {
            return;
          }

          const resultOut: Array<unknown> = [null];
          const awaitCode = awaitResult(handle, resultOut);
          finish(() => {
            tryDrop(drop, handle);
            if (typeof awaitCode !== "number" || awaitCode !== 0) {
              reject(new AsyncInteropError(`async await failed for ${abiExport.name}: code=${String(awaitCode)}`));
              return;
            }
            resolve(resultOut[0]);
          });
        } catch (error) {
          finish(() => {
            tryDrop(drop, handle);
            reject(new AsyncInteropError(`async export failed for ${abiExport.name}`, { cause: error }));
          });
        }
      }, pollIntervalMs);
      const timeout = setTimeout(() => {
        finish(() => {
          tryDrop(drop, handle);
          reject(
            new AsyncInteropError(
              `async export timed out for ${abiExport.name} after ${asyncTimeoutMs}ms (last done=${String(lastDoneValue)})`,
            ),
          );
        });
      }, asyncTimeoutMs);
    });
  } catch (error) {
    if (error instanceof AsyncInteropError) {
      throw error;
    }
    throw new SymbolLoadError(`failed binding async handle symbols for ${abiExport.name}`, { cause: error });
  }
}

function bindParamSpec(
  abiExport: AbiExport,
  paramName: string,
  registry: RuntimeTypeRegistry,
) {
  const param = abiExport.params.find((item) => item.name === paramName);
  if (!param) {
    throw new SymbolLoadError(`missing parameter ${paramName} on ${abiExport.name}`);
  }
  return koffiTypeForParam(param, abiExport, registry);
}

function renderPrototypeParam(
  abiExport: AbiExport,
  paramName: string,
  registry: RuntimeTypeRegistry,
): string {
  const param = abiExport.params.find((item) => item.name === paramName);
  if (!param) {
    throw new SymbolLoadError(`missing parameter ${paramName} on ${abiExport.name}`);
  }
  bindParamSpec(abiExport, paramName, registry);
  const qualifier =
    param.contract.ownership === "out"
      ? "_Out_ "
      : param.contract.ownership === "inout"
        ? "_Inout_ "
        : "";
  return `${qualifier}${param.c} ${param.name}`;
}

function tryDrop(drop: (...args: unknown[]) => unknown, handle: bigint | number): void {
  try {
    drop(handle);
  } catch {
    // Best-effort cleanup.
  }
}

function isCompletionSignal(value: unknown): boolean {
  return value === 1 || value === 1n || value === true;
}
