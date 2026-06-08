import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { buildNativeBridge, ensureTs2fzyArtifacts } from "../server/native.ts";

const MODEL_WEIGHT_COUNT = 51;

interface NativeFn {
  call: (...args: unknown[]) => unknown;
}

function expectNativeExport(name: string, value: unknown): NativeFn {
  if (!value || typeof value !== "object" || typeof (value as { call?: unknown }).call !== "function") {
    throw new Error(`native module did not expose ${name}`);
  }
  return value as NativeFn;
}

function expectNativeOk(name: string, status: unknown): void {
  if (typeof status !== "number" || !Number.isFinite(status)) {
    throw new Error(`${name} returned a non-numeric status`);
  }
  if (status !== 0) {
    throw new Error(`${name} failed with native status ${status}`);
  }
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

function readStats(buffer: Buffer) {
  const view = new Float32Array(buffer.buffer, buffer.byteOffset, 4);
  return {
    loss: view[0] ?? 0,
    accuracy: view[1] ?? 0,
    step: view[2] ?? 0,
    pointCount: view[3] ?? 0,
  };
}

function round(value: number): number {
  return Number(value.toFixed(6));
}

function summarizeFrame(buffer: Buffer) {
  let byteSum = 0;
  let nonZeroBytes = 0;
  for (const byte of buffer.values()) {
    byteSum += byte;
    if (byte !== 0) {
      nonZeroBytes += 1;
    }
  }
  return {
    byteSum,
    nonZeroBytes,
    prefix: Array.from(buffer.subarray(0, 16)),
  };
}

async function main(): Promise<void> {
  const testRoot = dirname(fileURLToPath(import.meta.url));
  const repoRoot = resolve(testRoot, "..", "..", "..");

  await ensureTs2fzyArtifacts();
  const { loadFozzyModule } = await import(resolve(repoRoot, "ts2fzy", "dist", "src", "runtime", "loader.js"));
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

  try {
    const randomizeModel = expectNativeExport("randomize_model", nativeModule.exports.get("randomize_model"));
    const evaluateModel = expectNativeExport("evaluate_model", nativeModule.exports.get("evaluate_model"));
    const trainSteps = expectNativeExport("train_steps", nativeModule.exports.get("train_steps"));
    const renderField = expectNativeExport("render_field", nativeModule.exports.get("render_field"));

    const weights = new Float32Array(MODEL_WEIGHT_COUNT);
    const weightBytes = Buffer.from(weights.buffer, weights.byteOffset, weights.byteLength);
    expectNativeOk("randomize_model", randomizeModel.call(weightBytes, bytesForView(weightBytes), MODEL_WEIGHT_COUNT, 1337));

    const points = new Float32Array([
      -0.85, -0.75,
      -0.55, -0.15,
      0.72, -0.18,
      0.82, -0.72,
      -0.05, 0.75,
      0.22, 0.64,
    ]);
    const classes = new Int32Array([0, 0, 1, 1, 2, 2]);
    const pointBytes = Buffer.from(points.buffer, points.byteOffset, points.byteLength);
    const classBytes = Buffer.from(classes.buffer, classes.byteOffset, classes.byteLength);

    const beforeStatsBuffer = Buffer.alloc(4 * 4);
    expectNativeOk(
      "evaluate_model",
      evaluateModel.call(
        weightBytes,
        bytesForView(weightBytes),
        MODEL_WEIGHT_COUNT,
        pointBytes,
        bytesForView(pointBytes),
        classes.length,
        classBytes,
        bytesForView(classBytes),
        classes.length,
        beforeStatsBuffer,
        bytesForView(beforeStatsBuffer),
        4,
      ),
    );
    const before = readStats(beforeStatsBuffer);

    const afterTrainStatsBuffer = Buffer.alloc(4 * 4);
    expectNativeOk(
      "train_steps",
      trainSteps.call(
        weightBytes,
        bytesForView(weightBytes),
        MODEL_WEIGHT_COUNT,
        pointBytes,
        bytesForView(pointBytes),
        classes.length,
        classBytes,
        bytesForView(classBytes),
        classes.length,
        96,
        0.04,
        afterTrainStatsBuffer,
        bytesForView(afterTrainStatsBuffer),
        4,
      ),
    );
    const afterTrain = readStats(afterTrainStatsBuffer);

    const afterEvalStatsBuffer = Buffer.alloc(4 * 4);
    expectNativeOk(
      "evaluate_model",
      evaluateModel.call(
        weightBytes,
        bytesForView(weightBytes),
        MODEL_WEIGHT_COUNT,
        pointBytes,
        bytesForView(pointBytes),
        classes.length,
        classBytes,
        bytesForView(classBytes),
        classes.length,
        afterEvalStatsBuffer,
        bytesForView(afterEvalStatsBuffer),
        4,
      ),
    );
    const afterEval = readStats(afterEvalStatsBuffer);

    if (!Number.isFinite(afterEval.loss) || !Number.isFinite(afterEval.accuracy)) {
      throw new Error("post-training stats were not finite");
    }
    if (afterEval.pointCount !== classes.length) {
      throw new Error(`expected ${classes.length} points but native stats reported ${afterEval.pointCount}`);
    }
    if (afterEval.loss > before.loss) {
      throw new Error(`training regression: loss rose from ${before.loss} to ${afterEval.loss}`);
    }

    const width = 32;
    const height = 32;
    const pixelCount = width * height;
    const coords = buildFieldCoords(width, height);
    const coordBytes = Buffer.from(coords.buffer, coords.byteOffset, coords.byteLength);
    const frame = Buffer.alloc(pixelCount * 4);
    expectNativeOk(
      "render_field",
      renderField.call(
        coordBytes,
        bytesForView(coordBytes),
        pixelCount * 2,
        weightBytes,
        bytesForView(weightBytes),
        MODEL_WEIGHT_COUNT,
        0,
        0,
        7,
        frame,
        frame.byteLength,
        pixelCount,
      ),
    );

    const frameSummary = summarizeFrame(frame);
    if (frameSummary.nonZeroBytes === 0) {
      throw new Error("rendered frame was all zeros");
    }

    process.stdout.write(`${JSON.stringify({
      before: {
        loss: round(before.loss),
        accuracy: round(before.accuracy),
        pointCount: before.pointCount,
      },
      afterTrain: {
        loss: round(afterTrain.loss),
        accuracy: round(afterTrain.accuracy),
        step: afterTrain.step,
        pointCount: afterTrain.pointCount,
      },
      afterEval: {
        loss: round(afterEval.loss),
        accuracy: round(afterEval.accuracy),
        pointCount: afterEval.pointCount,
      },
      frame: frameSummary,
      weightPrefix: Array.from(weights.slice(0, 8)).map(round),
    })}\n`);
  } finally {
    nativeModule.dispose();
  }
}

void main().catch((error) => {
  process.stderr.write(`${error instanceof Error ? error.stack ?? error.message : String(error)}\n`);
  process.exitCode = 1;
});
