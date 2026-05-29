import { extname } from "node:path";

export function sharedLibraryExtensionForPlatform(platform: NodeJS.Platform): string {
  switch (platform) {
    case "darwin":
      return ".dylib";
    case "win32":
      return ".dll";
    default:
      return ".so";
  }
}

export function hasSharedLibraryExtension(path: string): boolean {
  const ext = extname(path).toLowerCase();
  return ext === ".dylib" || ext === ".dll" || ext === ".so";
}

export function sanitizePackageStem(value: string): string {
  return value
    .replace(/[^A-Za-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .toLowerCase() || "fozzy_package";
}

export function defaultSharedLibraryFileNameForPackage(
  packageName: string,
  stemOverride?: string,
  platform: NodeJS.Platform = process.platform,
): string {
  const stem = sanitizePackageStem(stemOverride ?? packageName);
  const ext = sharedLibraryExtensionForPlatform(platform);
  return platform === "win32" ? `${stem}${ext}` : `lib${stem}${ext}`;
}

export function defaultHeaderFileNameForPackage(
  packageName: string,
  stemOverride?: string,
): string {
  const stem = sanitizePackageStem(stemOverride ?? packageName);
  return `${stem}.h`;
}
