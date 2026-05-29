import type {
  AbiCallbackBinding,
  AbiExport,
  AbiExportContract,
  AbiParam,
  AbiReturn,
  FozzyAbiManifest,
  PanicBoundary,
} from "../types/abi.js";
import { AbiMismatchError } from "./errors.js";

export interface AbiCompatibilityReport {
  readonly ok: boolean;
  readonly issues: string[];
  readonly comparedExports: string[];
  readonly addedExports: string[];
}

export interface AbiCompatibilityOptions {
  readonly requireSamePackage?: boolean;
  readonly requireSamePanicBoundary?: boolean;
}

export function compareAbiManifests(
  current: FozzyAbiManifest,
  baseline: FozzyAbiManifest,
  options: AbiCompatibilityOptions = {},
): AbiCompatibilityReport {
  const requireSamePackage = options.requireSamePackage ?? true;
  const requireSamePanicBoundary = options.requireSamePanicBoundary ?? true;
  const issues: string[] = [];
  const comparedExports: string[] = [];
  const addedExports: string[] = [];

  if (current.schemaVersion !== baseline.schemaVersion) {
    issues.push(
      `schemaVersion mismatch: current=${current.schemaVersion} baseline=${baseline.schemaVersion}`,
    );
  }

  if (requireSamePackage) {
    if (current.package.name !== baseline.package.name) {
      issues.push(
        `package name mismatch: current=${current.package.name} baseline=${baseline.package.name}`,
      );
    }
    if (current.package.version !== baseline.package.version) {
      issues.push(
        `package version mismatch: current=${current.package.version} baseline=${baseline.package.version}`,
      );
    }
  }

  if (requireSamePanicBoundary) {
    comparePanicBoundary(current.panicBoundary, baseline.panicBoundary, issues);
  }

  const currentExports = new Map(current.exports.map((item) => [item.name, item]));
  const baselineExports = new Map(baseline.exports.map((item) => [item.name, item]));

  for (const [name, baselineExport] of baselineExports) {
    comparedExports.push(name);
    const currentExport = currentExports.get(name);
    if (!currentExport) {
      issues.push(`missing export in current ABI: ${name}`);
      continue;
    }

    if (normalizeExportSignature(currentExport) !== normalizeExportSignature(baselineExport)) {
      issues.push(
        `signature changed for export ${name}: current=${normalizeExportSignature(currentExport)} baseline=${normalizeExportSignature(baselineExport)}`,
      );
    }

    if (normalizeExportContract(currentExport.contract) !== normalizeExportContract(baselineExport.contract)) {
      issues.push(`contract changed for export ${name}`);
    }

    if (
      normalizeParamContracts(currentExport.params) !== normalizeParamContracts(baselineExport.params)
    ) {
      issues.push(`parameter contract changed for export ${name}`);
    }

    if (
      normalizeReturnContract(currentExport.return) !== normalizeReturnContract(baselineExport.return)
    ) {
      issues.push(`return contract changed for export ${name}`);
    }

    if (currentExport.symbolVersion < baselineExport.symbolVersion) {
      issues.push(
        `symbolVersion regressed for export ${name}: current=${currentExport.symbolVersion} baseline=${baselineExport.symbolVersion}`,
      );
    }
  }

  for (const currentExport of current.exports) {
    if (!baselineExports.has(currentExport.name)) {
      addedExports.push(currentExport.name);
    }
  }

  return {
    ok: issues.length === 0,
    issues,
    comparedExports,
    addedExports,
  };
}

export function assertAbiCompatible(
  current: FozzyAbiManifest,
  baseline: FozzyAbiManifest,
  options: AbiCompatibilityOptions = {},
): void {
  const report = compareAbiManifests(current, baseline, options);
  if (!report.ok) {
    throw new AbiMismatchError(report.issues.join("; "));
  }
}

function comparePanicBoundary(
  current: PanicBoundary,
  baseline: PanicBoundary,
  issues: string[],
): void {
  if (current !== baseline) {
    issues.push(`panicBoundary mismatch: current=${current} baseline=${baseline}`);
  }
}

function normalizeExportSignature(abiExport: AbiExport): string {
  const params = abiExport.params.map((param) => `${param.name}:${param.c}`).join(",");
  return `${abiExport.name}(${params})->${abiExport.return.c}#${abiExport.contract.execution}`;
}

function normalizeParamContracts(params: AbiParam[]): string {
  return JSON.stringify(
    params.map((param) => ({
      name: param.name,
      contract: param.contract,
    })),
  );
}

function normalizeReturnContract(ret: AbiReturn): string {
  return JSON.stringify(ret.contract);
}

function normalizeExportContract(contract: AbiExportContract): string {
  return JSON.stringify({
    execution: contract.execution,
    callbackBindings: contract.callbackBindings.map(normalizeCallbackBinding),
    asyncBoundary: contract.asyncBoundary,
  });
}

function normalizeCallbackBinding(binding: AbiCallbackBinding) {
  return {
    callbackParam: binding.callbackParam,
    contextParam: binding.contextParam,
    bindingId: binding.bindingId,
    obligation: binding.obligation,
    lifetime: binding.lifetime,
    signature: binding.signature,
  };
}
