import { accessSync, constants, readFileSync } from "node:fs";

import koffi, { type LibraryHandle } from "koffi";

import type { AbiExport, FozzyAbiManifest } from "../types/abi.js";
import type { LoadModuleOptions, RegisteredCallbackHandle as PublicRegisteredCallbackHandle } from "../types/public.js";
import { createDiagnosticEmitter, type DiagnosticEmitter } from "./diagnostics.js";
import {
  AsyncInteropError,
  NativeBoundaryError,
  OwnershipError,
  ResourceStateError,
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
import {
  compileCallArgAdapter,
  compileResultValueAdapter,
  type CallArgAdapter,
  type OwnedPointerReleaserMap,
  type ResultValueAdapter,
} from "./ownership.js";

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

interface ExportPlan {
  readonly abi: AbiExport;
  readonly callArgAdapter: CallArgAdapter;
  readonly returnNormalizer: (value: unknown) => unknown;
}

interface ManifestPlan {
  readonly manifest: FozzyAbiManifest;
  readonly exports: ReadonlyMap<string, ExportPlan>;
}

const MANIFEST_PLAN_CACHE = new Map<string, ManifestPlan>();

export function loadFozzyModule(options: LoadModuleOptions): LoadedFozzyModule {
  const emit = createDiagnosticEmitter(options.diagnostics);
  const emitEnabled = emit.enabled;
  if (emitEnabled) {
    emit({
      kind: "module.load.start",
      message: "Loading Fozzy module",
      detail: {
        sharedLibrary: options.paths.sharedLibrary,
        abiManifest: options.paths.abiManifest,
      },
    });
  }
  ensureReadable(options.paths.abiManifest);
  ensureReadable(options.paths.sharedLibrary);

  const manifestText = readFileSync(options.paths.abiManifest, "utf8");
  const manifestPlan = getOrCreateManifestPlan(manifestText);
  const manifest = manifestPlan.manifest;
  if (emitEnabled) {
    emit({
      kind: "module.manifest.parsed",
      message: "Parsed ABI manifest",
      detail: {
        packageName: manifest.package.name,
        packageVersion: manifest.package.version,
        exportCount: manifest.exports.length,
      },
    });
  }
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
  if (emitEnabled) {
    emit({
      kind: "module.library.loaded",
      message: "Loaded shared library",
      detail: {
        sharedLibrary: options.paths.sharedLibrary,
      },
    });
  }
  const registry = buildRuntimeTypeRegistry(manifest);
  const loadedExports = new Map<string, LoadedExport>();
  const activeCallbacks = new Set<RegisteredCallbackHandle>();
  const pollIntervalMs = options.pollIntervalMs ?? 5;
  const asyncTimeoutMs = options.asyncTimeoutMs ?? 5_000;
  const releasers = options.ownedPointerReleasers;
  let disposed = false;

  for (const abiExport of manifest.exports) {
    const exportPlan = manifestPlan.exports.get(abiExport.name);
    if (!exportPlan) {
      throw new SymbolLoadError(`missing compiled export plan for ${abiExport.name}`);
    }

    let rawCall: ((...args: unknown[]) => unknown) | null = null;
    let asyncBinding: AsyncHandleRuntimeBinding | null = null;
    let resultValueAdapter: ResultValueAdapter | null = null;
    try {
      rawCall = abiExport.async ? null : bindKoffiFunction(library, abiExport, registry);
      asyncBinding = abiExport.async
        ? bindAsyncHandleRuntimeBinding(abiExport, manifest, library, registry)
        : null;
      resultValueAdapter = compileResultValueAdapter(abiExport, library, registry, releasers);
      if (emitEnabled) {
        emit({
          kind: "module.export.bound",
          message: "Bound export",
          detail: {
            exportName: abiExport.name,
            async: abiExport.async,
            callbackBindings: abiExport.contract.callbackBindings.length,
          },
        });
      }
    } catch (error) {
      if (error instanceof SymbolLoadError || error instanceof AsyncInteropError) {
        throw error;
      }
      throw new SymbolLoadError(`failed binding export ${abiExport.name}`, { cause: error });
    }
    const loaded: LoadedExport = {
      abi: abiExport,
      call(...args: unknown[]) {
        if (disposed) {
          throw new ResourceStateError(`loaded module ${manifest.package.name} is already disposed`);
        }
        if (abiExport.async) {
          if (asyncBinding === null) {
            throw new AsyncInteropError(`async export ${abiExport.name} did not bind correctly`);
          }
          if (resultValueAdapter === null) {
            throw new NativeBoundaryError(`result adapter for ${abiExport.name} did not bind correctly`);
          }
          try {
            const adaptedArgs = exportPlan.callArgAdapter(args);
            return invokeAsyncHandleExport(
              asyncBinding,
              abiExport,
              adaptedArgs,
              pollIntervalMs,
              asyncTimeoutMs,
              resultValueAdapter,
              exportPlan.returnNormalizer,
              emitEnabled ? emit : undefined,
            );
          } catch (error) {
            if (emitEnabled) {
              emit({
                kind: "async.failed",
                message: "Async export failed before start",
                detail: {
                  exportName: abiExport.name,
                },
              });
            }
            throw new NativeBoundaryError(`native async call failed for ${abiExport.name}`, { cause: error });
          }
        }
        if (rawCall === null) {
          throw new NativeBoundaryError(`sync export ${abiExport.name} did not bind correctly`);
        }
        try {
          if (emitEnabled) {
            emit({
              kind: "sync.call.started",
              message: "Started sync export call",
              detail: {
                exportName: abiExport.name,
              },
            });
          }
          const adaptedArgs = exportPlan.callArgAdapter(args);
          const result = rawCall(...adaptedArgs);
          const adaptedResult = resultValueAdapter?.(result) ?? result;
          const normalizedResult = exportPlan.returnNormalizer(adaptedResult);
          if (emitEnabled) {
            emit({
              kind: "sync.call.completed",
              message: "Completed sync export call",
              detail: {
                exportName: abiExport.name,
              },
            });
          }
          return normalizedResult;
        } catch (error) {
          if (emitEnabled) {
            emit({
              kind: "sync.call.failed",
              message: "Sync export call failed",
              detail: {
                exportName: abiExport.name,
              },
            });
          }
          throw new NativeBoundaryError(`native call failed for ${abiExport.name}`, { cause: error });
        }
      },
      registerCallback(bindingId: string, fn: (...args: unknown[]) => unknown) {
        if (disposed) {
          throw new ResourceStateError(`loaded module ${manifest.package.name} is already disposed`);
        }
        if (typeof fn !== "function") {
          throw new OwnershipError(`callback registration for ${abiExport.name} requires a function`);
        }
        const binding = abiExport.contract.callbackBindings.find((item) => item.bindingId === bindingId);
        if (!binding) {
          throw new OwnershipError(`callback binding ${bindingId} does not exist on ${abiExport.name}`);
        }
        const handle = registerCallback(
          binding,
          (...callbackArgs: unknown[]) => {
            if (emitEnabled) {
              emit({
                kind: "callback.invoked",
                message: "Invoked callback binding",
                detail: {
                  exportName: abiExport.name,
                  bindingId,
                },
              });
            }
            try {
              const result = fn(...callbackArgs);
              if (emitEnabled) {
                emit({
                  kind: "callback.completed",
                  message: "Completed callback binding",
                  detail: {
                    exportName: abiExport.name,
                    bindingId,
                  },
                });
              }
              return result;
            } catch (error) {
              if (emitEnabled) {
                emit({
                  kind: "callback.failed",
                  message: "Callback binding threw",
                  detail: {
                    exportName: abiExport.name,
                    bindingId,
                  },
                });
              }
              throw error;
            }
          },
          registry,
        );
        if (emitEnabled) {
          emit({
            kind: "callback.registered",
            message: "Registered callback binding",
            detail: {
              exportName: abiExport.name,
              bindingId,
              pointer: handle.pointer.toString(),
            },
          });
        }
        activeCallbacks.add(handle);
        let callbackDisposed = false;
        return {
          pointer: handle.pointer,
          bindingId: handle.binding.bindingId,
          exportName: abiExport.name,
          get disposed() {
            return callbackDisposed;
          },
          dispose() {
            if (callbackDisposed) {
              return;
            }
            callbackDisposed = true;
            handle.dispose();
            activeCallbacks.delete(handle);
            if (emitEnabled) {
              emit({
                kind: "callback.disposed",
                message: "Disposed callback binding",
                detail: {
                  exportName: abiExport.name,
                  bindingId,
                },
              });
            }
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
      if (disposed) {
        return;
      }
      disposed = true;
      for (const handle of activeCallbacks) {
        handle.dispose();
      }
      activeCallbacks.clear();
      library.unload();
      if (emitEnabled) {
        emit({
          kind: "module.disposed",
          message: "Disposed loaded module",
          detail: {
            packageName: manifest.package.name,
          },
        });
      }
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
  resultValueAdapter: ResultValueAdapter,
  returnNormalizer: (value: unknown) => unknown,
  emit?: DiagnosticEmitter,
): Promise<unknown> {
  try {
    return invokeAsyncHandleRuntimeBinding(binding, abiExport.name, args, pollIntervalMs, asyncTimeoutMs, emit).then(
      (result) => {
        const adaptedResult = resultValueAdapter(result);
        return returnNormalizer(adaptedResult);
      },
    );
  } catch (error) {
    if (error instanceof AsyncInteropError) {
      throw error;
    }
    throw new SymbolLoadError(`failed binding async handle symbols for ${abiExport.name}`, { cause: error });
  }
}

function getOrCreateManifestPlan(manifestText: string): ManifestPlan {
  const cached = MANIFEST_PLAN_CACHE.get(manifestText);
  if (cached) {
    return cached;
  }

  const manifest = parseAbiManifest(manifestText);
  const exports = new Map<string, ExportPlan>();
  for (const abiExport of manifest.exports) {
    assertSupportedExportSubset(abiExport);
    exports.set(abiExport.name, {
      abi: abiExport,
      callArgAdapter: compileCallArgAdapter(abiExport),
      returnNormalizer: compileJsValueNormalizer(abiExport.return.c, manifest),
    });
  }

  const plan: ManifestPlan = {
    manifest,
    exports,
  };
  MANIFEST_PLAN_CACHE.set(manifestText, plan);
  return plan;
}

function compileJsValueNormalizer(
  cType: string,
  manifest: FozzyAbiManifest,
): (value: unknown) => unknown {
  const normalizedBaseType = cType.replace(/\bconst\b/g, "").replace(/\*/g, "").trim().replace(/\s+/g, " ");
  if (
    normalizedBaseType === "int64_t" ||
    normalizedBaseType === "uint64_t" ||
    normalizedBaseType === "size_t" ||
    normalizedBaseType === "ssize_t" ||
    normalizedBaseType === "fz_async_handle_t"
  ) {
    return (value: unknown) => (typeof value === "number" ? BigInt(value) : value);
  }

  const layout = manifest.reprCLayouts.find((item) => item.name === normalizedBaseType);
  if (!layout || layout.kind !== "struct") {
    return (value: unknown) => value;
  }

  const fieldNormalizers = (layout.fields ?? []).map((field) => ({
    name: field.name,
    normalize: compileJsValueNormalizer(field.c, manifest),
  }));
  return (value: unknown) => {
    if (typeof value !== "object" || value === null || Array.isArray(value)) {
      return value;
    }

    const input = value as Record<string, unknown>;
    const output: Record<string, unknown> = {};
    for (const field of fieldNormalizers) {
      output[field.name] = field.normalize(input[field.name]);
    }
    return output;
  };
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
