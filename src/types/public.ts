export interface Disposable {
  dispose(): void;
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

export interface LoadModuleOptions {
  paths: LibraryPaths;
  package?: PackageIdentity;
  strict?: boolean;
  pollIntervalMs?: number;
}

export interface GeneratedBindingOptions {
  runtimeImportPath?: string;
  exportName?: string;
}
