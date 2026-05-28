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
  lines.push(`import type { Disposable, LoadModuleOptions, OpaqueHandle } from "${runtimeImportPath}";`);
  lines.push(`import { loadFozzyModule } from "${runtimeImportPath}";`);
  lines.push("");
  for (const handle of model.handleTypes) {
    lines.push(`export interface ${handle.typeName} extends OpaqueHandle<${JSON.stringify(handle.brand)}> {`);
    lines.push("  readonly pointer: unknown;");
    lines.push("  readonly disposed: boolean;");
    lines.push("  decodeBytes(length: number): Uint8Array;");
    if (handle.supportsStringDecode) {
      lines.push("  decodeString(): string;");
    }
    lines.push("}");
    lines.push("");
  }
  lines.push(`export interface ${sanitizeIdentifier(manifest.package.name)}Bindings {`);
  lines.push("  dispose(): void;");
  for (const item of model.exports) {
    const params = item.tsParams.map((param) => `${param.jsName}: ${param.tsType}`).join(", ");
    lines.push(`  ${item.jsName}(${params}): ${item.tsReturnType};`);
  }
  for (const callback of model.callbacks) {
    lines.push(`  ${callback.methodName}(fn: ${callback.callbackType}): Disposable;`);
  }
  lines.push("}");
  lines.push("");
  lines.push(`export function ${exportName}(options: LoadModuleOptions): ${sanitizeIdentifier(manifest.package.name)}Bindings {`);
  lines.push("  const module = loadFozzyModule(options);");
  lines.push("  return {");
  lines.push("    dispose() {");
  lines.push("      module.dispose();");
  lines.push("    },");
  for (const item of model.exports) {
    const argNames = item.tsParams.map((param) => param.jsName).join(", ");
    lines.push(`    ${item.jsName}(${argNames}) {`);
    lines.push(`      const fn = module.exports.get(${JSON.stringify(item.abiName)});`);
    lines.push(`      if (!fn) throw new Error(${JSON.stringify(`missing export binding for ${item.abiName}`)});`);
      lines.push(`      return fn.call(${argNames}) as ${item.tsReturnType};`);
    lines.push("    },");
  }
  for (const callback of model.callbacks) {
    lines.push(`    ${callback.methodName}(fn) {`);
    lines.push(`      const binding = module.exports.get(${JSON.stringify(callback.exportAbiName)});`);
    lines.push(`      if (!binding) throw new Error(${JSON.stringify(`missing export binding for ${callback.exportAbiName}`)});`);
    lines.push(`      return binding.registerCallback(${JSON.stringify(callback.bindingId)}, fn);`);
    lines.push("    },");
  }
  lines.push("  };");
  lines.push("}");
  lines.push("");
  return `${lines.join("\n")}\n`;
}

export function renderBindingJavaScript(
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): string {
  const runtimeImportPath = options.runtimeImportPath ?? "j2fz";
  const exportName = options.exportName ?? "createBindings";
  const model = buildGeneratedModuleModel(manifest, exportName);

  const lines: string[] = [];
  lines.push(`import { loadFozzyModule } from "${runtimeImportPath}";`);
  lines.push("");
  lines.push(`export function ${exportName}(options) {`);
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
  lines.push("    dispose() {");
  lines.push("      module.dispose();");
  lines.push("    },");
  for (const callback of model.callbacks) {
    lines.push(`    ${callback.methodName}(fn) {`);
    lines.push(`      const binding = module.exports.get(${JSON.stringify(callback.exportAbiName)});`);
    lines.push(`      if (!binding) throw new Error(${JSON.stringify(`missing export binding for ${callback.exportAbiName}`)});`);
    lines.push(`      return binding.registerCallback(${JSON.stringify(callback.bindingId)}, fn);`);
    lines.push("    },");
  }
  lines.push("  };");
  lines.push("}");
  lines.push("");
  return `${lines.join("\n")}\n`;
}

export function renderBindingTypes(
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): string {
  const runtimeImportPath = options.runtimeImportPath ?? "j2fz";
  const exportName = options.exportName ?? "createBindings";
  const model = buildGeneratedModuleModel(manifest, exportName);

  const lines: string[] = [];
  lines.push(`import type { Disposable, LoadModuleOptions, OpaqueHandle } from "${runtimeImportPath}";`);
  lines.push("");
  for (const handle of model.handleTypes) {
    lines.push(`export interface ${handle.typeName} extends OpaqueHandle<${JSON.stringify(handle.brand)}> {`);
    lines.push("  readonly pointer: unknown;");
    lines.push("  readonly disposed: boolean;");
    lines.push("  decodeBytes(length: number): Uint8Array;");
    if (handle.supportsStringDecode) {
      lines.push("  decodeString(): string;");
    }
    lines.push("}");
    lines.push("");
  }
  lines.push(`export interface ${sanitizeIdentifier(manifest.package.name)}Bindings {`);
  lines.push("  dispose(): void;");
  for (const item of model.exports) {
    const params = item.tsParams.map((param) => `${param.jsName}: ${param.tsType}`).join(", ");
    lines.push(`  ${item.jsName}(${params}): ${item.tsReturnType};`);
  }
  for (const callback of model.callbacks) {
    lines.push(`  ${callback.methodName}(fn: ${callback.callbackType}): Disposable;`);
  }
  lines.push("}");
  lines.push("");
  lines.push(`export function ${exportName}(options: LoadModuleOptions): ${sanitizeIdentifier(manifest.package.name)}Bindings;`);
  lines.push("");
  return `${lines.join("\n")}\n`;
}

export function renderGeneratedPackageJson(
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): string {
  const packageName = options.packageName ?? defaultGeneratedPackageName(manifest.package.name);
  const packageVersion = options.packageVersion ?? manifest.package.version;
  const packageDescription =
    options.packageDescription ?? `Generated j2fz bindings for ${manifest.package.name}`;
  const runtimeImportPath = options.runtimeImportPath ?? "j2fz";
  const packageJson: Record<string, unknown> = {
    name: packageName,
    version: packageVersion,
    private: true,
    description: packageDescription,
    type: "module",
    main: "./index.js",
    types: "./index.d.ts",
    exports: {
      ".": {
        import: "./index.js",
        types: "./index.d.ts",
      },
      "./abi.manifest.json": "./abi.manifest.json",
    },
    files: ["index.js", "index.d.ts", "abi.manifest.json", "README.md"],
    keywords: ["fozzy", "ffi", "j2fz", "generated-bindings"],
  };
  if (!runtimeImportPath.startsWith(".") && !runtimeImportPath.startsWith("/")) {
    packageJson.dependencies = {
      [runtimeImportPath]: "*",
    };
  }
  return `${JSON.stringify(packageJson, null, 2)}\n`;
}

export function renderGeneratedReadme(
  manifest: FozzyAbiManifest,
  options: GeneratedBindingOptions = {},
): string {
  const exportName = options.exportName ?? "createBindings";
  const model = buildGeneratedModuleModel(manifest, exportName);
  const lines: string[] = [];
  lines.push(`# ${options.packageName ?? defaultGeneratedPackageName(manifest.package.name)}`);
  lines.push("");
  lines.push(`Generated j2fz bindings for the Fozzy package \`${manifest.package.name}\`.`);
  lines.push("");
  lines.push("## Exports");
  lines.push("");
  for (const item of model.exports) {
    const params = item.tsParams.map((param) => `\`${param.jsName}: ${param.tsType}\``).join(", ");
    lines.push(`- \`${item.jsName}(${params}) => ${item.tsReturnType}\``);
  }
  if (model.callbacks.length > 0) {
    lines.push("");
    lines.push("## Callback Registrations");
    lines.push("");
    for (const callback of model.callbacks) {
      lines.push(`- \`${callback.methodName}(fn: ${callback.callbackType})\``);
    }
  }
  lines.push("");
  lines.push("## Usage");
  lines.push("");
  lines.push("```ts");
  lines.push(`import { ${exportName} } from ".";`);
  lines.push("");
  lines.push(`const bindings = ${exportName}({`);
  lines.push("  paths: {");
  lines.push('    sharedLibrary: "/absolute/path/to/library",');
  lines.push('    abiManifest: "/absolute/path/to/abi.manifest.json",');
  lines.push("  },");
  lines.push("});");
  lines.push("```");
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

function defaultGeneratedPackageName(packageName: string): string {
  return `${packageName.replace(/[^A-Za-z0-9._-]/g, "-")}-j2fz`;
}
