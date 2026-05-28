import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

import type { FozzyAbiManifest } from "../types/abi.js";
import type { GeneratedBindingOptions } from "../types/public.js";
import { renderBindingModule } from "./render.js";

export interface GenerateBindingsResult {
  readonly modulePath: string;
}

export function writeGeneratedBindings(
  outDir: string,
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): GenerateBindingsResult {
  const modulePath = join(outDir, "index.ts");
  mkdirSync(dirname(modulePath), { recursive: true });
  writeFileSync(modulePath, renderBindingModule(manifest, options), "utf8");
  return { modulePath };
}
