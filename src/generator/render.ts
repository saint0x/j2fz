import type { GeneratedBindingOptions } from "../types/public.js";
import type { FozzyAbiManifest } from "../types/abi.js";
import { buildGeneratedModuleModel } from "./model.js";

export function renderBindingModule(
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): string {
  const runtimeImportPath = options.runtimeImportPath ?? "j2fz";
  const exportName = options.exportName ?? "createBindings";
  const model = buildGeneratedModuleModel(manifest, exportName);

  const lines: string[] = [];
  lines.push(`import type { LoadModuleOptions } from "${runtimeImportPath}";`);
  lines.push(`import { loadFozzyModule } from "${runtimeImportPath}";`);
  lines.push("");
  lines.push("export interface OpaqueHandle<TBrand extends string> {");
  lines.push("  readonly __brand: TBrand;");
  lines.push("}");
  lines.push("");
  lines.push(`export interface ${sanitizeIdentifier(manifest.package.name)}Bindings {`);
  for (const item of model.exports) {
    const params = item.tsParams.map((param) => `${param.jsName}: ${param.tsType}`).join(", ");
    lines.push(`  ${item.jsName}(${params}): ${item.tsReturnType};`);
  }
  lines.push("}");
  lines.push("");
  lines.push(`export function ${exportName}(options: LoadModuleOptions): ${sanitizeIdentifier(manifest.package.name)}Bindings {`);
  lines.push("  const module = loadFozzyModule(options);");
  lines.push("  return {");
  for (const item of model.exports) {
    const argNames = item.tsParams.map((param) => param.jsName).join(", ");
    lines.push(`    ${item.jsName}(${argNames}) {`);
    lines.push(`      const fn = module.exports.get(${JSON.stringify(item.abiName)});`);
    lines.push(`      if (!fn) throw new Error(${JSON.stringify(`missing export binding for ${item.abiName}`)});`);
    lines.push(`      return fn.call(${argNames});`);
    lines.push("    },");
  }
  lines.push("  };");
  lines.push("}");
  lines.push("");
  return `${lines.join("\n")}\n`;
}

function sanitizeIdentifier(value: string): string {
  const rewritten = value.replace(/[^A-Za-z0-9_$]/g, "_");
  if (/^[0-9]/.test(rewritten)) {
    return `_${rewritten}`;
  }
  return rewritten;
}
