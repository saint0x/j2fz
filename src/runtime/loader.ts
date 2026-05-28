import { accessSync, constants, readFileSync } from "node:fs";

import koffi, { type LibraryHandle } from "koffi";

import type { AbiExport, FozzyAbiManifest } from "../types/abi.js";
import type { LoadModuleOptions, RegisteredCallbackHandle as PublicRegisteredCallbackHandle } from "../types/public.js";
import { createDiagnosticEmitter, type DiagnosticEmitter } from "./diagnostics.js";
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
  registerCallback(bindingId: string, fn: (...args: unknown[]) => unknown): PublicRegisteredCallbackHandle;
}

export interface AsyncHandleRuntimeBinding {
  start(...args: unknown[]): unknown;
  poll(handle: bigint | number, doneOut: Array<number | bigint | boolean | null>): unknown;
  awaitResult(handle: bigint | number, resultOut: Array<unknown>): unknown;
  drop(handle: bigint | number): unknown;
}

export function loadFozzyModule(options: LoadModuleOptions): LoadedFozzyModule {
  const emit = createDiagnosticEmitter(options.diagnostics);
  emit({
    kind: "module.load.start",
    message: "Loading Fozzy module",
    detail: {
      sharedLibrary: options.paths.sharedLibrary,
      abiManifest: options.paths.abiManifest,
    },
  });
  ensureReadable(options.paths.abiManifest);
  ensureReadable(options.paths.sharedLibrary);

  const manifestText = readFileSync(options.paths.abiManifest, "utf8");
  const manifest = parseAbiManifest(manifestText);
  emit({
    kind: "module.manifest.parsed",
    message: "Parsed ABI manifest",
    detail: {
      packageName: manifest.package.name,
      packageVersion: manifest.package.version,
      exportCount: manifest.exports.length,
    },
  });
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
  emit({
    kind: "module.library.loaded",
    message: "Loaded shared library",
    detail: {
      sharedLibrary: options.paths.sharedLibrary,
    },
  });
  const registry = buildRuntimeTypeRegistry(manifest);
  const loadedExports = new Map<string, LoadedExport>();
  const activeCallbacks = new Set<RegisteredCallbackHandle>();
  const pollIntervalMs = options.pollIntervalMs ?? 5;
  const asyncTimeoutMs = options.asyncTimeoutMs ?? 5_000;
  const releasers = options.ownedPointerReleasers;

  for (const abiExport of manifest.exports) {
    assertSupportedExportSubset(abiExport);

    let rawCall: ((...args: unknown[]) => unknown) | null = null;
    let asyncBinding: AsyncHandleRuntimeBinding | null = null;
    try {
      rawCall = abiExport.async ? null : bindKoffiFunction(library, abiExport, registry);
      asyncBinding = abiExport.async
        ? bindAsyncHandleRuntimeBinding(abiExport, manifest, library, registry)
        : null;
      emit({
        kind: "module.export.bound",
        message: "Bound export",
        detail: {
          exportName: abiExport.name,
          async: abiExport.async,
          callbackBindings: abiExport.contract.callbackBindings.length,
        },
      });
    } catch (error) {
      if (error instanceof SymbolLoadError || error instanceof AsyncInteropError) {
        throw error;
      }
      throw new SymbolLoadError(`failed binding export ${abiExport.name}`, { cause: error });
    }
    const loaded: LoadedExport = {
      abi: abiExport,
      call(...args: unknown[]) {
        if (abiExport.async) {
          if (asyncBinding === null) {
            throw new AsyncInteropError(`async export ${abiExport.name} did not bind correctly`);
          }
          return invokeAsyncHandleExport(
            asyncBinding,
            abiExport,
            args,
            pollIntervalMs,
            asyncTimeoutMs,
            emit,
          );
        }
        if (rawCall === null) {
          throw new NativeBoundaryError(`sync export ${abiExport.name} did not bind correctly`);
        }
        try {
          emit({
            kind: "sync.call.started",
            message: "Started sync export call",
            detail: {
              exportName: abiExport.name,
            },
          });
          const adaptedArgs = adaptCallArgs(abiExport, args);
          const result = rawCall(...adaptedArgs);
          const adaptedResult = adaptResultValue(abiExport, result, library, registry, releasers);
          emit({
            kind: "sync.call.completed",
            message: "Completed sync export call",
            detail: {
              exportName: abiExport.name,
            },
          });
          return adaptedResult;
        } catch (error) {
          emit({
            kind: "sync.call.failed",
            message: "Sync export call failed",
            detail: {
              exportName: abiExport.name,
            },
          });
          throw new NativeBoundaryError(`native call failed for ${abiExport.name}`, { cause: error });
        }
      },
      registerCallback(bindingId: string, fn: (...args: unknown[]) => unknown) {
        const binding = abiExport.contract.callbackBindings.find((item) => item.bindingId === bindingId);
        if (!binding) {
          throw new OwnershipError(`callback binding ${bindingId} does not exist on ${abiExport.name}`);
        }
        const handle = registerCallback(
          binding,
          (...callbackArgs: unknown[]) => {
            emit({
              kind: "callback.invoked",
              message: "Invoked callback binding",
              detail: {
                exportName: abiExport.name,
                bindingId,
              },
            });
            try {
              const result = fn(...callbackArgs);
              emit({
                kind: "callback.completed",
                message: "Completed callback binding",
                detail: {
                  exportName: abiExport.name,
                  bindingId,
                },
              });
              return result;
            } catch (error) {
              emit({
                kind: "callback.failed",
                message: "Callback binding threw",
                detail: {
                  exportName: abiExport.name,
                  bindingId,
                },
              });
              throw error;
            }
          },
          registry,
        );
        emit({
          kind: "callback.registered",
          message: "Registered callback binding",
          detail: {
            exportName: abiExport.name,
            bindingId,
            pointer: handle.pointer.toString(),
          },
        });
        activeCallbacks.add(handle);
        return {
          pointer: handle.pointer,
          bindingId: handle.binding.bindingId,
          exportName: abiExport.name,
          dispose() {
            handle.dispose();
            activeCallbacks.delete(handle);
            emit({
              kind: "callback.disposed",
              message: "Disposed callback binding",
              detail: {
                exportName: abiExport.name,
                bindingId,
              },
            });
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
      emit({
        kind: "module.disposed",
        message: "Disposed loaded module",
        detail: {
          packageName: manifest.package.name,
        },
      });
    },
  };
}

function ensureReadable(path: string): void {
  accessSync(path, constants.R_OK);
}

function invokeAsyncHandleExport(
  binding: AsyncHandleRuntimeBinding,
  abiExport: AbiExport,
  args: unknown[],
  pollIntervalMs: number,
  asyncTimeoutMs: number,
  emit?: DiagnosticEmitter,
): Promise<unknown> {
  try {
    return invokeAsyncHandleRuntimeBinding(binding, abiExport.name, args, pollIntervalMs, asyncTimeoutMs, emit);
  } catch (error) {
    if (error instanceof AsyncInteropError) {
      throw error;
    }
    throw new SymbolLoadError(`failed binding async handle symbols for ${abiExport.name}`, { cause: error });
  }
}

function bindAsyncHandleRuntimeBinding(
  abiExport: AbiExport,
  manifest: FozzyAbiManifest,
  library: LibraryHandle,
  registry: RuntimeTypeRegistry,
): AsyncHandleRuntimeBinding {
  const boundary = abiExport.contract.asyncBoundary;
  if (boundary === null) {
    throw new AsyncInteropError(`async export ${abiExport.name} is missing async boundary`);
  }

  const parent = manifest.exports.find((item) => item.name === abiExport.name);
  if (!parent) {
    throw new SymbolLoadError(`missing parent export ${abiExport.name}`);
  }

  try {
    return {
      start: library.func(
        `int32_t ${boundary.startSymbol}(${[
          ...parent.params.map((param) => renderPrototypeParam(parent, param.name, registry)),
          "_Out_ uint64_t *handle_out",
        ].join(", ")})`,
      ),
      poll: library.func(`int32_t ${boundary.pollSymbol}(uint64_t handle, _Out_ int32_t *done_out)`),
      awaitResult: library.func(
        `int32_t ${boundary.awaitSymbol}(uint64_t handle, _Out_ ${boundary.resultType} *result_out)`,
      ),
      drop: library.func(`int32_t ${boundary.dropSymbol}(uint64_t handle)`),
    };
  } catch (error) {
    throw new SymbolLoadError(`failed binding async handle symbols for ${abiExport.name}`, { cause: error });
  }
}

export function invokeAsyncHandleRuntimeBinding(
  binding: AsyncHandleRuntimeBinding,
  exportName: string,
  args: unknown[],
  pollIntervalMs: number,
  asyncTimeoutMs: number,
  emit?: DiagnosticEmitter,
): Promise<unknown> {
  const handleOut: Array<bigint | number | null> = [null];
  const startCode = binding.start(...args, handleOut);
  if (typeof startCode !== "number" || startCode !== 0) {
    throw new AsyncInteropError(`async start failed for ${exportName}: code=${String(startCode)}`);
  }
  const handle = handleOut[0];
  if (typeof handle !== "bigint" && typeof handle !== "number") {
    throw new AsyncInteropError(`async start did not return a valid handle for ${exportName}`);
  }
  emit?.({
    kind: "async.started",
    message: "Started async export",
    detail: {
      exportName,
      handle: handle.toString(),
    },
  });

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
        const doneOut: Array<number | bigint | boolean | null> = [null];
        const pollCode = binding.poll(handle, doneOut);
        if (typeof pollCode !== "number" || pollCode !== 0) {
          finish(() => {
            tryDrop(binding.drop, handle);
            emit?.({
              kind: "async.poll_failed",
              message: "Async export poll failed",
              detail: {
                exportName,
                handle: handle.toString(),
                code: pollCode,
              },
            });
            reject(new AsyncInteropError(`async poll failed for ${exportName}: code=${String(pollCode)}`));
          });
          return;
        }
        lastDoneValue = doneOut[0];
        if (!isCompletionSignal(doneOut[0])) {
          return;
        }

        const resultOut: Array<unknown> = [null];
        const awaitCode = binding.awaitResult(handle, resultOut);
        finish(() => {
          tryDrop(binding.drop, handle);
          if (typeof awaitCode !== "number" || awaitCode !== 0) {
            emit?.({
              kind: "async.await_failed",
              message: "Async export await failed",
              detail: {
                exportName,
                handle: handle.toString(),
                code: awaitCode,
              },
            });
            reject(new AsyncInteropError(`async await failed for ${exportName}: code=${String(awaitCode)}`));
            return;
          }
          emit?.({
            kind: "async.completed",
            message: "Async export completed",
            detail: {
              exportName,
              handle: handle.toString(),
            },
          });
          resolve(resultOut[0]);
        });
      } catch (error) {
        finish(() => {
          tryDrop(binding.drop, handle);
          emit?.({
            kind: "async.failed",
            message: "Async export failed",
            detail: {
              exportName,
              handle: handle.toString(),
            },
          });
          reject(new AsyncInteropError(`async export failed for ${exportName}`, { cause: error }));
        });
      }
    }, pollIntervalMs);
      const timeout = setTimeout(() => {
        finish(() => {
          tryDrop(binding.drop, handle);
          emit?.({
            kind: "async.timed_out",
            message: "Async export timed out",
            detail: {
              exportName,
              handle: handle.toString(),
              timeoutMs: asyncTimeoutMs,
              lastDoneValue: lastDoneValue === null ? null : String(lastDoneValue),
            },
          });
          reject(
            new AsyncInteropError(
              `async export timed out for ${exportName} after ${asyncTimeoutMs}ms (last done=${String(lastDoneValue)})`,
          ),
        );
      });
    }, asyncTimeoutMs);
  });
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

function tryDrop(drop: (handle: bigint | number) => unknown, handle: bigint | number): void {
  try {
    drop(handle);
  } catch {
    // Best-effort cleanup.
  }
}

function isCompletionSignal(value: unknown): boolean {
  return value === 1 || value === 1n || value === true;
}
