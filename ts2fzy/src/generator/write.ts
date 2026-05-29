import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

import type { FozzyAbiManifest } from "../types/abi.js";
import type { GeneratedBindingOptions } from "../types/public.js";
import {
  renderBindingJavaScript,
  renderBindingModule,
  renderBindingTypes,
  renderGeneratedPackageJson,
  renderGeneratedReadme,
} from "./render.js";

export interface GenerateBindingsResult {
  readonly packageDir: string;
  readonly modulePath: string;
  readonly typesPath: string;
  readonly packageJsonPath: string;
  readonly manifestPath: string | null;
  readonly readmePath: string | null;
}

export function writeGeneratedBindings(
  outDir: string,
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): GenerateBindingsResult {
  const modulePath = join(outDir, "index.js");
  const typesPath = join(outDir, "index.d.ts");
  const packageJsonPath = join(outDir, "package.json");
  const manifestPath = options.emitManifestCopy === false ? null : join(outDir, "abi.manifest.json");
  const readmePath = options.emitReadme === false ? null : join(outDir, "README.md");
  mkdirSync(dirname(modulePath), { recursive: true });
  writeFileSync(modulePath, renderBindingJavaScript(manifest, options), "utf8");
  writeFileSync(typesPath, renderBindingTypes(manifest, options), "utf8");
  writeFileSync(packageJsonPath, renderGeneratedPackageJson(manifest, options), "utf8");
  if (manifestPath !== null) {
    writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
  }
  if (readmePath !== null) {
    writeFileSync(readmePath, renderGeneratedReadme(manifest, options), "utf8");
  }
  writeFileSync(join(outDir, "index.ts"), renderBindingModule(manifest, options), "utf8");
  return {
    packageDir: outDir,
    modulePath,
    typesPath,
    packageJsonPath,
    manifestPath,
    readmePath,
  };
}
