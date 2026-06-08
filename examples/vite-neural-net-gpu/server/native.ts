import { access } from "node:fs/promises";
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

const exampleRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(exampleRoot, "..", "..");
const fozzyRoot = resolve(repoRoot, "..", "fozzylang");
const fzyagentRoot = resolve(repoRoot, "..", "fzyagent");
const nativeProjectRoot = join(exampleRoot, "native");
const ts2fzyRoot = join(repoRoot, "ts2fzy");
const preferredLocalFz = join(fzyagentRoot, ".local", "bin", "fz");
const localTsc = join(ts2fzyRoot, "node_modules", ".bin", "tsc");

interface NativeBuildOutput {
  sharedLib: string;
  abiManifest: string;
}

async function runChecked(command: string, args: string[], cwd: string): Promise<string> {
  const { stdout, stderr } = await execFileAsync(command, args, { cwd });
  if (stderr.trim().length > 0) {
    process.stderr.write(stderr);
  }
  return stdout;
}

async function runCompiler(args: string[], cwd: string): Promise<string> {
  if (process.env.J2FZ_FZ_BIN) {
    return await runChecked(process.env.J2FZ_FZ_BIN, args, cwd);
  }
  if (existsSync(fozzyRoot)) {
    try {
      return await runChecked("cargo", ["run", "-q", "-p", "fz", "--", ...args], fozzyRoot);
    } catch {
      // Fall through to local binaries as the next-best option.
    }
  }
  if (existsSync(preferredLocalFz)) {
    return await runChecked(preferredLocalFz, args, cwd);
  }
  return await runChecked("fz", args, cwd);
}

export async function ensureTs2fzyArtifacts(): Promise<void> {
  await access(ts2fzyRoot);
  if (existsSync(localTsc)) {
    await runChecked(localTsc, ["-p", "tsconfig.json"], ts2fzyRoot);
    await runChecked("bash", ["./scripts/build-host-shim.sh"], ts2fzyRoot);
    return;
  }
  await runChecked("npm", ["run", "build"], ts2fzyRoot);
  await runChecked("npm", ["run", "build:host-shim"], ts2fzyRoot);
}

export async function buildNativeBridge(): Promise<NativeBuildOutput> {
  const raw = await runCompiler(["build", nativeProjectRoot, "--lib", "--json"], fozzyRoot);
  const payload = JSON.parse(raw) as Partial<NativeBuildOutput> & { status?: string };
  if (payload.status && payload.status !== "ok") {
    throw new Error(`native bridge build failed with status ${payload.status}`);
  }
  if (!payload.sharedLib || !payload.abiManifest) {
    throw new Error("native bridge build did not return sharedLib and abiManifest");
  }
  return {
    sharedLib: payload.sharedLib,
    abiManifest: payload.abiManifest,
  };
}

export function examplePath(...parts: string[]): string {
  return join(exampleRoot, ...parts);
}
