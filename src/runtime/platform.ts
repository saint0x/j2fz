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
