import { accessSync, constants, existsSync } from "node:fs";
import { join } from "node:path";

import type {
  EmbeddedLibraryPaths,
  LibraryPaths,
  LoadEmbeddedPackageOptions,
  LoadPackageOptions,
  PackageArtifactDiscoveryOptions,
} from "../types/public.js";
import { createDiagnosticEmitter } from "./diagnostics.js";
import { SymbolLoadError } from "./errors.js";
import {
  defaultHeaderFileNameForPackage,
  defaultSharedLibraryFileNameForPackage,
} from "./platform.js";
import {
  loadFozzyModule,
  loadFozzyModuleWithManifest,
  type LoadedFozzyModule,
} from "./loader.js";

const ENV_SHARED_LIBRARY = "J2FZ_SHARED_LIBRARY";
const ENV_ABI_MANIFEST = "J2FZ_ABI_MANIFEST";
const ENV_HEADER = "J2FZ_HEADER";

export function discoverLibraryPaths(options: PackageArtifactDiscoveryOptions): LibraryPaths {
  const resolved = resolveDiscoveredArtifacts(options, true);
  if (resolved.abiManifest === undefined) {
    throw new SymbolLoadError("ABI manifest discovery did not resolve a readable path");
  }
  return resolved as LibraryPaths;
}

export function discoverEmbeddedLibraryPaths(options: PackageArtifactDiscoveryOptions): EmbeddedLibraryPaths {
  const resolved = resolveDiscoveredArtifacts(options, false);
  const embedded: EmbeddedLibraryPaths = {
    sharedLibrary: resolved.sharedLibrary,
  };
  if (resolved.header !== undefined) {
    embedded.header = resolved.header;
  }
  return embedded;
}

export function loadFozzyPackage(options: LoadPackageOptions): LoadedFozzyModule {
  const emit = createDiagnosticEmitter(options.diagnostics);
  const paths = discoverLibraryPaths(options.discovery);
  emit({
    kind: "package.discovery.resolved",
    message: "Resolved generated package artifacts",
    detail: {
      packageRoot: options.discovery.packageRoot,
      sharedLibrary: paths.sharedLibrary,
      abiManifest: paths.abiManifest,
      header: paths.header ?? null,
    },
  });
  return loadFozzyModule({
    ...options,
    paths,
  });
}

export function loadFozzyPackageWithManifest(options: LoadEmbeddedPackageOptions): LoadedFozzyModule {
  const emit = createDiagnosticEmitter(options.diagnostics);
  const paths = discoverEmbeddedLibraryPaths(options.discovery);
  emit({
    kind: "package.discovery.resolved",
    message: "Resolved generated package artifacts",
    detail: {
      packageRoot: options.discovery.packageRoot,
      sharedLibrary: paths.sharedLibrary,
      abiManifest: null,
      header: paths.header ?? null,
    },
  });
  return loadFozzyModuleWithManifest({
    ...options,
    paths,
  });
}

function resolveDiscoveredArtifacts(
  options: PackageArtifactDiscoveryOptions,
  requireAbiManifest: boolean,
): Partial<LibraryPaths> & EmbeddedLibraryPaths {
  const env = options.env ?? process.env;
  const packageName = options.package?.name ?? "fozzy_package";
  const nativeDir = join(options.packageRoot, options.nativeDir ?? "native");
  const includeDir = join(options.packageRoot, options.includeDir ?? "include");

  const sharedLibrary =
    options.sharedLibraryFileName !== undefined
      ? join(nativeDir, options.sharedLibraryFileName)
      : env[ENV_SHARED_LIBRARY] ?? join(
          nativeDir,
          defaultSharedLibraryFileNameForPackage(
            packageName,
            options.sharedLibraryStem,
          ),
        );

  const abiManifest =
    options.abiManifestFileName !== undefined
      ? join(options.packageRoot, options.abiManifestFileName)
      : env[ENV_ABI_MANIFEST] ?? join(options.packageRoot, "abi.manifest.json");

  const explicitHeaderPath =
    options.headerFileName !== undefined
      ? join(includeDir, options.headerFileName)
      : env[ENV_HEADER] ?? join(includeDir, defaultHeaderFileNameForPackage(packageName));

  ensureReadable(sharedLibrary, "shared library");
  if (requireAbiManifest) {
    ensureReadable(abiManifest, "ABI manifest");
  }

  const resolved: Partial<LibraryPaths> & EmbeddedLibraryPaths = {
    sharedLibrary,
  };
  if (requireAbiManifest) {
    resolved.abiManifest = abiManifest;
  }
  if (existsSync(explicitHeaderPath)) {
    resolved.header = explicitHeaderPath;
  }
  return resolved;
}

function ensureReadable(path: string, label: string): void {
  try {
    accessSync(path, constants.R_OK);
  } catch (error) {
    throw new SymbolLoadError(`${label} is not readable at ${path}`, { cause: error });
  }
}
