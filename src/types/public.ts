export interface Disposable {
  dispose(): void;
}

export interface OpaqueHandle<TBrand extends string> extends Disposable {
  readonly __brand: TBrand;
}

export interface PackageIdentity {
  name: string;
  version: string;
}

export interface LibraryPaths {
  sharedLibrary: string;
  abiManifest: string;
  header?: string;
}

export interface PackageArtifactDiscoveryOptions {
  packageRoot: string;
  package?: PackageIdentity;
  nativeDir?: string;
  includeDir?: string;
  sharedLibraryFileName?: string;
  sharedLibraryStem?: string;
  abiManifestFileName?: string;
  headerFileName?: string;
  env?: NodeJS.ProcessEnv;
}

export interface LoadModuleOptions {
  paths: LibraryPaths;
  package?: PackageIdentity;
  strict?: boolean;
  pollIntervalMs?: number;
  asyncTimeoutMs?: number;
  ownedPointerReleasers?: Record<string, string>;
}

export interface GeneratedBindingOptions {
  runtimeImportPath?: string;
  exportName?: string;
  packageName?: string;
  packageVersion?: string;
  packageDescription?: string;
  emitManifestCopy?: boolean;
  emitReadme?: boolean;
}

export interface LoadPackageOptions extends Omit<LoadModuleOptions, "paths"> {
  discovery: PackageArtifactDiscoveryOptions;
}
