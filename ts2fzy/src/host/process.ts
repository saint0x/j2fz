import { access } from "node:fs/promises";
import { spawn, type ChildProcess } from "node:child_process";

export interface SpawnJsHostServerProcessOptions {
  socketPath: string;
  nodeExecutable?: string;
  startupTimeoutMs?: number;
}

export interface JsHostServerProcessHandle {
  readonly socketPath: string;
  readonly child: ChildProcess;
  close(): Promise<void>;
}

function cliPath(): string {
  return new URL("./cli.js", import.meta.url).pathname;
}

async function waitForSocket(socketPath: string, timeoutMs: number): Promise<void> {
  const startedAt = Date.now();
  for (;;) {
    try {
      await access(socketPath);
      return;
    } catch {
      if (Date.now() - startedAt > timeoutMs) {
        throw new Error(`timed out waiting for JS host socket at ${socketPath}`);
      }
      await new Promise((resolve) => setTimeout(resolve, 25));
    }
  }
}

export async function spawnJsHostServerProcess(
  options: SpawnJsHostServerProcessOptions,
): Promise<JsHostServerProcessHandle> {
  const child = spawn(options.nodeExecutable ?? process.execPath, [cliPath(), options.socketPath], {
    stdio: ["ignore", "ignore", "pipe"],
  });
  let stderr = "";
  child.stderr?.setEncoding("utf8");
  child.stderr?.on("data", (chunk: string) => {
    stderr += chunk;
  });
  await waitForSocket(options.socketPath, options.startupTimeoutMs ?? 5_000);
  return {
    socketPath: options.socketPath,
    child,
    async close(): Promise<void> {
      if (child.exitCode !== null) {
        return;
      }
      child.kill("SIGTERM");
      await new Promise<void>((resolve, reject) => {
        child.once("exit", () => resolve());
        child.once("error", reject);
        setTimeout(() => {
          if (child.exitCode === null) {
            child.kill("SIGKILL");
          }
        }, 200);
      });
      if (stderr.trim().length > 0 && child.exitCode !== 0) {
        throw new Error(`JS host server stderr: ${stderr.trim()}`);
      }
    },
  };
}
