import http from "node:http";
import { readFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { tmpdir } from "node:os";

import { createServer as createViteServer } from "vite";

import { bridgeModulePath, buildNativeBridge, ensureTs2fzyArtifacts } from "./native.ts";

const exampleRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const isProduction = process.env.NODE_ENV === "production";
const port = Number(process.env.PORT ?? 4318);
const host = process.env.HOST ?? "127.0.0.1";

interface BridgeClickResult {
  nativeLifted: number;
  handshakeScore: number;
}

function decodeSigned16(value: number): number {
  const masked = value & 0xffff;
  return masked >= 0x8000 ? masked - 0x10000 : masked;
}

function unpackBridgeClickResult(packed: number): BridgeClickResult {
  return {
    nativeLifted: decodeSigned16(packed),
    handshakeScore: packed >> 16,
  };
}

async function jsonResponse(
  response: http.ServerResponse,
  statusCode: number,
  payload: unknown,
): Promise<void> {
  const body = JSON.stringify(payload, null, 2);
  response.writeHead(statusCode, {
    "content-type": "application/json; charset=utf-8",
    "content-length": Buffer.byteLength(body),
  });
  response.end(body);
}

async function serveStaticIndex(response: http.ServerResponse): Promise<void> {
  const indexPath = join(exampleRoot, "dist", "index.html");
  const html = await readFile(indexPath);
  response.writeHead(200, {
    "content-type": "text/html; charset=utf-8",
    "content-length": html.length,
  });
  response.end(html);
}

async function main(): Promise<void> {
  await ensureTs2fzyArtifacts();
  const { loadFozzyModule } = await import("../../../ts2fzy/dist/src/runtime/loader.js");
  const { spawnJsHostServerProcess } = await import("../../../ts2fzy/dist/src/host/process.js");
  const build = await buildNativeBridge();
  const socketPath = join(tmpdir(), `j2fz-vite-click-${process.pid}.sock`);
  const bridgeModule = bridgeModulePath();
  const jsHost = await spawnJsHostServerProcess({ socketPath });

  process.env.J2FZ_JS_HOST_SOCKET = socketPath;
  process.env.J2FZ_EXAMPLE_MODULE = bridgeModule;

  const nativeModule = loadFozzyModule({
    paths: {
      sharedLibrary: build.sharedLib,
      abiManifest: build.abiManifest,
    },
    package: {
      name: "j2fz_vite_click_bridge",
      version: "0.1.0",
    },
  });

  const bridgeClick = nativeModule.exports.get("bridge_click");
  if (!bridgeClick) {
    throw new Error("native module did not expose bridge_click");
  }

  let clickCount = 0;
  const vite = isProduction
    ? null
    : await createViteServer({
        root: exampleRoot,
        server: {
          middlewareMode: true,
          host,
          port,
        },
      });

  const server = http.createServer(async (request, response) => {
    try {
      if (request.url === "/api/click" && request.method === "POST") {
        clickCount += 1;
        const packed = bridgeClick.call(clickCount) as number;
        const result = unpackBridgeClickResult(packed);
        await jsonResponse(response, 200, {
          clickCount,
          inputCount: clickCount,
          jsDoubled: result.nativeLifted - 10,
          nativeLifted: result.nativeLifted,
          handshakeScore: result.handshakeScore,
          bridgeModule,
        });
        return;
      }

      if (!isProduction && vite) {
        vite.middlewares(request, response, () => {
          response.statusCode = 404;
          response.end("not found");
        });
        return;
      }

      if (isProduction && request.url === "/") {
        await serveStaticIndex(response);
        return;
      }

      const assetPath = join(exampleRoot, "dist", request.url === "/" ? "index.html" : request.url ?? "");
      if (isProduction && existsSync(assetPath)) {
        const content = await readFile(assetPath);
        response.writeHead(200);
        response.end(content);
        return;
      }

      response.statusCode = 404;
      response.end("not found");
    } catch (error) {
      await jsonResponse(response, 500, {
        error: error instanceof Error ? error.message : String(error),
      });
    }
  });

  server.listen(port, host, () => {
    process.stdout.write(`vite-click-bridge listening on http://${host}:${port}\n`);
  });

  let shuttingDown: Promise<void> | null = null;
  const shutdown = async (): Promise<void> => {
    if (shuttingDown) {
      return await shuttingDown;
    }
    shuttingDown = (async () => {
      nativeModule.dispose();
      await jsHost.close();
      await vite?.close();
      await new Promise<void>((resolveClose) => {
        server.close(() => resolveClose());
      });
    })();
    return await shuttingDown;
  };

  process.once("SIGINT", () => {
    void shutdown();
  });
  process.once("SIGTERM", () => {
    void shutdown();
  });
}

void main().catch((error) => {
  process.stderr.write(`${error instanceof Error ? error.stack ?? error.message : String(error)}\n`);
  process.exitCode = 1;
});
