import http from "node:http";
import { readFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { createServer as createViteServer } from "vite";

import { buildNativeBridge, ensureTs2fzyArtifacts, examplePath } from "./native.ts";

const exampleRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const isProduction = process.env.NODE_ENV === "production";
const port = Number(process.env.PORT ?? 4328);
const host = process.env.HOST ?? "127.0.0.1";

const FIELD_WIDTH = 112;
const FIELD_HEIGHT = 112;
const MODEL_WEIGHT_COUNT = 51;
const CLASS_COLORS = [
  { id: 0, label: "Red Ink", hex: "#f05a68" },
  { id: 1, label: "Blue Ink", hex: "#54a7ff" },
  { id: 2, label: "Gold Ink", hex: "#f4bd4a" },
];

type RenderMode = "class" | "ascii" | "neuron";

interface PointRecord {
  x: number;
  y: number;
  classId: number;
}

interface ModelStats {
  loss: number;
  accuracy: number;
  step: number;
  pointCount: number;
}

interface NativeGarden {
  randomizeModel: { call: (...args: unknown[]) => unknown };
  evaluateModel: { call: (...args: unknown[]) => unknown };
  trainSteps: { call: (...args: unknown[]) => unknown };
  renderField: { call: (...args: unknown[]) => unknown };
}

const weights = new Float32Array(MODEL_WEIGHT_COUNT);
const points: PointRecord[] = [];
let trainingStep = 0;
let lastStats: ModelStats = { loss: 0, accuracy: 0, step: 0, pointCount: 0 };
const fieldCoords = buildFieldCoords(FIELD_WIDTH, FIELD_HEIGHT);

function modeToNative(mode: RenderMode): number {
  if (mode === "ascii") {
    return 1;
  }
  if (mode === "neuron") {
    return 2;
  }
  return 0;
}

function weightsBuffer(): Buffer {
  return Buffer.from(weights.buffer, weights.byteOffset, weights.byteLength);
}

function bytesForView(view: ArrayBufferView): number {
  return view.byteLength;
}

function buildFieldCoords(width: number, height: number): Float32Array {
  const coords = new Float32Array(width * height * 2);
  let index = 0;
  for (let y = 0; y < height; y += 1) {
    const normalizedY = ((y / Math.max(1, height - 1)) * 2) - 1;
    for (let x = 0; x < width; x += 1) {
      const normalizedX = ((x / Math.max(1, width - 1)) * 2) - 1;
      coords[index] = normalizedX;
      coords[index + 1] = normalizedY;
      index += 2;
    }
  }
  return coords;
}

function pointsBuffer(): Float32Array {
  const raw = new Float32Array(points.length * 2);
  for (let index = 0; index < points.length; index += 1) {
    raw[index * 2] = points[index]!.x;
    raw[(index * 2) + 1] = points[index]!.y;
  }
  return raw;
}

function classesBuffer(): Int32Array {
  const raw = new Int32Array(points.length);
  for (let index = 0; index < points.length; index += 1) {
    raw[index] = points[index]!.classId;
  }
  return raw;
}

function statsFromBuffer(buffer: Buffer): Omit<ModelStats, "step"> {
  const view = new Float32Array(buffer.buffer, buffer.byteOffset, 4);
  return {
    loss: view[0] ?? 0,
    accuracy: view[1] ?? 0,
    pointCount: Math.round(view[3] ?? 0),
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

function errorDetails(error: unknown): unknown {
  if (!(error instanceof Error)) {
    return { message: String(error) };
  }
  const details: Record<string, unknown> = {
    name: error.name,
    message: error.message,
    stack: error.stack ?? error.message,
  };
  const cause = (error as Error & { cause?: unknown }).cause;
  if (cause !== undefined) {
    details.cause = errorDetails(cause);
  }
  return details;
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

async function readJsonBody<T>(request: http.IncomingMessage): Promise<T> {
  const chunks: Buffer[] = [];
  for await (const chunk of request) {
    chunks.push(typeof chunk === "string" ? Buffer.from(chunk) : chunk);
  }
  const text = Buffer.concat(chunks).toString("utf8");
  if (text.trim().length === 0) {
    return {} as T;
  }
  return JSON.parse(text) as T;
}

function normalizePoint(value: number): number {
  return Math.max(-1, Math.min(1, value));
}

function lessonSteps(): Array<{ id: string; title: string; body: string; focus: string[] }> {
  return [
    {
      id: "inputs",
      title: "Inputs",
      body: "Every pixel and every training click becomes two numbers: x and y in normalized space.",
      focus: ["x", "y", "normalized coordinate"],
    },
    {
      id: "weights",
      title: "Weights",
      body: "The first layer weights decide which directions in space each hidden neuron cares about.",
      focus: ["W1", "b1", "weights"],
    },
    {
      id: "hidden",
      title: "Hidden Neurons",
      body: "Each of the 8 hidden neurons carves out a soft curved region of the plane.",
      focus: ["hidden", "neuron", "activation"],
    },
    {
      id: "activation",
      title: "Activation",
      body: "The tanh nonlinearity bends straight affine math into curved boundaries that can wrap around your clicks.",
      focus: ["fast_tanh", "tanh"],
    },
    {
      id: "outputs",
      title: "Outputs",
      body: "The 3 output neurons compete, and the largest class score wins each pixel.",
      focus: ["logits", "softmax", "argmax3"],
    },
    {
      id: "loss",
      title: "Loss",
      body: "Cross-entropy loss measures how surprised the model is by the correct class for each point.",
      focus: ["loss_for_class", "fast_log", "softmax"],
    },
    {
      id: "training",
      title: "Training",
      body: "Manual backprop updates W2, b2, then W1 and b1 so the right regions grow around your examples.",
      focus: ["train_steps", "dlogits", "dz"],
    },
    {
      id: "gpu",
      title: "GPU Kernel",
      body: "The GPU runs the forward pass once per pixel in parallel and paints the decision field all at once.",
      focus: ["kernel fn forward_field", "gpu.global_id_x", "gpu.launch4"],
    },
    {
      id: "bridge",
      title: "j2fz Bridge",
      body: "The browser drives the experiment, Node owns the buffers, and Fzy owns the neural math and GPU render path.",
      focus: ["renderField", "trainSteps", "loadFozzyModule"],
    },
  ];
}

function groupedWeights(): Array<{ name: string; values: number[] }> {
  return [
    { name: "W1(x)", values: Array.from(weights.slice(0, 8)) },
    { name: "W1(y)", values: Array.from(weights.slice(8, 16)) },
    { name: "b1", values: Array.from(weights.slice(16, 24)) },
    { name: "W2(red)", values: Array.from(weights.slice(24, 32)) },
    { name: "W2(blue)", values: Array.from(weights.slice(32, 40)) },
    { name: "W2(gold)", values: Array.from(weights.slice(40, 48)) },
    { name: "b2", values: Array.from(weights.slice(48, 51)) },
  ];
}

async function sourceTabs(): Promise<Record<string, string>> {
  const [nativeSource, serverSource, appSource, specSource] = await Promise.all([
    readFile(examplePath("native", "src", "lib.fzy"), "utf8"),
    readFile(examplePath("server", "app.ts"), "utf8"),
    readFile(examplePath("src", "app.ts"), "utf8"),
    readFile(examplePath("SPEC.md"), "utf8"),
  ]);
  const entries = [
    ["native/src/lib.fzy", nativeSource],
    ["server/app.ts", serverSource],
    ["src/app.ts", appSource],
    ["SPEC.md", specSource],
  ] as const;
  return Object.fromEntries(entries);
}

function expectNativeOk(name: string, status: unknown): void {
  if (typeof status !== "number" || !Number.isFinite(status)) {
    throw new Error(`${name} returned a non-numeric status`);
  }
  if (status !== 0) {
    throw new Error(`${name} failed with native status ${status}`);
  }
}

async function refreshStats(native: NativeGarden): Promise<ModelStats> {
  if (points.length === 0) {
    lastStats = {
      loss: 0,
      accuracy: 0,
      pointCount: 0,
      step: trainingStep,
    };
    return lastStats;
  }
  const statsBuffer = Buffer.alloc(4 * 4);
  const xy = pointsBuffer();
  const classes = classesBuffer();
  const weightBytes = weightsBuffer();
  const xyBytes = Buffer.from(xy.buffer, xy.byteOffset, xy.byteLength);
  const classBytes = Buffer.from(classes.buffer, classes.byteOffset, classes.byteLength);
  const status = native.evaluateModel.call(
    weightBytes,
    bytesForView(weightBytes),
    MODEL_WEIGHT_COUNT,
    xyBytes,
    bytesForView(xyBytes),
    points.length,
    classBytes,
    bytesForView(classBytes),
    points.length,
    statsBuffer,
    bytesForView(statsBuffer),
    4,
  );
  expectNativeOk("evaluate_model", status);
  const partial = statsFromBuffer(statsBuffer);
  lastStats = {
    loss: partial.loss,
    accuracy: partial.accuracy,
    pointCount: partial.pointCount,
    step: trainingStep,
  };
  return lastStats;
}

function validateNativeExport(name: string, value: unknown): { call: (...args: unknown[]) => unknown } {
  if (!value || typeof value !== "object" || typeof (value as { call?: unknown }).call !== "function") {
    throw new Error(`native module did not expose ${name}`);
  }
  return value as { call: (...args: unknown[]) => unknown };
}

async function main(): Promise<void> {
  await ensureTs2fzyArtifacts();
  const { loadFozzyModule } = await import("../../../ts2fzy/dist/src/runtime/loader.js");
  const build = await buildNativeBridge();
  const nativeModule = loadFozzyModule({
    paths: {
      sharedLibrary: build.sharedLib,
      abiManifest: build.abiManifest,
    },
    package: {
      name: "j2fz_vite_neural_ink_garden",
      version: "0.1.0",
    },
  });

  const native: NativeGarden = {
    randomizeModel: validateNativeExport("randomize_model", nativeModule.exports.get("randomize_model")),
    evaluateModel: validateNativeExport("evaluate_model", nativeModule.exports.get("evaluate_model")),
    trainSteps: validateNativeExport("train_steps", nativeModule.exports.get("train_steps")),
    renderField: validateNativeExport("render_field", nativeModule.exports.get("render_field")),
  };

  {
    const weightBytes = weightsBuffer();
    expectNativeOk(
      "randomize_model",
      native.randomizeModel.call(weightBytes, bytesForView(weightBytes), MODEL_WEIGHT_COUNT, 1337),
    );
  }
  await refreshStats(native);

  const sourceText = await sourceTabs();
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
      const path = request.url ?? "/";
      if (path === "/api/bootstrap" && request.method === "GET") {
        await jsonResponse(response, 200, {
          width: FIELD_WIDTH,
          height: FIELD_HEIGHT,
          classOptions: CLASS_COLORS,
          steps: lessonSteps(),
          weights: groupedWeights(),
          stats: lastStats,
          points,
          sourceTabs: sourceText,
        });
        return;
      }

      if (path === "/api/add-point" && request.method === "POST") {
        const payload = await readJsonBody<{ x: number; y: number; classId: number }>(request);
        if (points.length >= 256) {
          await jsonResponse(response, 400, { error: "max 256 training points" });
          return;
        }
        points.push({
          x: normalizePoint(Number(payload.x ?? 0)),
          y: normalizePoint(Number(payload.y ?? 0)),
          classId: Math.max(0, Math.min(2, Number(payload.classId ?? 0))),
        });
        await refreshStats(native);
        await jsonResponse(response, 200, { ok: true, points, stats: lastStats });
        return;
      }

      if (path === "/api/clear-points" && request.method === "POST") {
        points.splice(0, points.length);
        trainingStep = 0;
        await refreshStats(native);
        await jsonResponse(response, 200, { ok: true, points, stats: lastStats });
        return;
      }

      if (path === "/api/randomize-model" && request.method === "POST") {
        const payload = await readJsonBody<{ seed?: number }>(request);
        {
          const weightBytes = weightsBuffer();
          expectNativeOk(
            "randomize_model",
            native.randomizeModel.call(
              weightBytes,
              bytesForView(weightBytes),
              MODEL_WEIGHT_COUNT,
              Number(payload.seed ?? 1337),
            ),
          );
        }
        trainingStep = 0;
        await refreshStats(native);
        await jsonResponse(response, 200, {
          ok: true,
          stats: lastStats,
          weights: groupedWeights(),
        });
        return;
      }

      if (path === "/api/train" && request.method === "POST") {
        const payload = await readJsonBody<{ count?: number; learningRate?: number }>(request);
        if (points.length === 0) {
          await refreshStats(native);
          await jsonResponse(response, 200, {
            ok: true,
            stats: lastStats,
            weights: groupedWeights(),
          });
          return;
        }
        const xy = pointsBuffer();
        const classes = classesBuffer();
        const statsBuffer = Buffer.alloc(4 * 4);
        const count = Math.max(1, Math.min(512, Number(payload.count ?? 1)));
        const learningRate = Math.max(0.0005, Math.min(0.5, Number(payload.learningRate ?? 0.04)));
        const weightBytes = weightsBuffer();
        const xyBytes = Buffer.from(xy.buffer, xy.byteOffset, xy.byteLength);
        const classBytes = Buffer.from(classes.buffer, classes.byteOffset, classes.byteLength);
        const trainStatus = native.trainSteps.call(
          weightBytes,
          bytesForView(weightBytes),
          MODEL_WEIGHT_COUNT,
          xyBytes,
          bytesForView(xyBytes),
          points.length,
          classBytes,
          bytesForView(classBytes),
          points.length,
          count,
          learningRate,
          statsBuffer,
          bytesForView(statsBuffer),
          4,
        );
        expectNativeOk("train_steps", trainStatus);
        trainingStep += count;
        const partial = statsFromBuffer(statsBuffer);
        lastStats = {
          loss: partial.loss,
          accuracy: partial.accuracy,
          pointCount: partial.pointCount,
          step: trainingStep,
        };
        await jsonResponse(response, 200, {
          ok: true,
          stats: lastStats,
          weights: groupedWeights(),
        });
        return;
      }

      if (path === "/api/render" && request.method === "POST") {
        const payload = await readJsonBody<{ mode?: RenderMode; selectedNeuron?: number; timeTick?: number }>(request);
        const mode = payload.mode ?? "class";
        const pixelCount = FIELD_WIDTH * FIELD_HEIGHT;
        const frameBuffer = Buffer.alloc(pixelCount * 4);
        const coordBytes = Buffer.from(fieldCoords.buffer, fieldCoords.byteOffset, fieldCoords.byteLength);
        const weightBytes = weightsBuffer();
        const renderStatus = native.renderField.call(
          coordBytes,
          bytesForView(coordBytes),
          pixelCount * 2,
          weightBytes,
          bytesForView(weightBytes),
          MODEL_WEIGHT_COUNT,
          modeToNative(mode),
          Math.max(0, Math.min(7, Number(payload.selectedNeuron ?? 0))),
          Math.max(0, Number(payload.timeTick ?? 0)),
          frameBuffer,
          frameBuffer.byteLength,
          pixelCount,
        );
        expectNativeOk("render_field", renderStatus);
        await jsonResponse(response, 200, {
          width: FIELD_WIDTH,
          height: FIELD_HEIGHT,
          mode,
          frameBase64: frameBuffer.toString("base64"),
          points,
          stats: lastStats,
          weights: groupedWeights(),
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

      if (isProduction && path === "/") {
        await serveStaticIndex(response);
        return;
      }

      const assetPath = join(exampleRoot, "dist", path === "/" ? "index.html" : path);
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
        error: errorDetails(error),
      });
    }
  });

  server.listen(port, host, () => {
    process.stdout.write(`vite-neural-ink-garden listening on http://${host}:${port}\n`);
  });

  let shuttingDown: Promise<void> | null = null;
  const shutdown = async (): Promise<void> => {
    if (shuttingDown) {
      return await shuttingDown;
    }
    shuttingDown = (async () => {
      nativeModule.dispose();
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
