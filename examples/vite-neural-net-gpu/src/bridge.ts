import type { BootstrapPayload, RenderMode, RenderPayload } from "./state";

async function readJson<T>(response: Response): Promise<T> {
  if (!response.ok) {
    const message = await response.text();
    throw new Error(message || `request failed with ${response.status}`);
  }
  return (await response.json()) as T;
}

async function postJson<T>(path: string, payload: unknown): Promise<T> {
  const response = await fetch(path, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify(payload),
  });
  return await readJson<T>(response);
}

export async function fetchBootstrap(): Promise<BootstrapPayload> {
  const response = await fetch("/api/bootstrap");
  return await readJson<BootstrapPayload>(response);
}

export async function addPoint(x: number, y: number, classId: number) {
  return await postJson<{ ok: boolean; points: Array<{ x: number; y: number; classId: number }>; stats: { loss: number; accuracy: number; step: number; pointCount: number } }>(
    "/api/add-point",
    { x, y, classId },
  );
}

export async function clearPoints() {
  return await postJson<{ ok: boolean; points: Array<{ x: number; y: number; classId: number }>; stats: { loss: number; accuracy: number; step: number; pointCount: number } }>(
    "/api/clear-points",
    {},
  );
}

export async function randomizeModel(seed: number) {
  return await postJson<{ ok: boolean; stats: { loss: number; accuracy: number; step: number; pointCount: number }; weights: Array<{ name: string; values: number[] }> }>(
    "/api/randomize-model",
    { seed },
  );
}

export async function trainModel(count: number, learningRate: number) {
  return await postJson<{ ok: boolean; stats: { loss: number; accuracy: number; step: number; pointCount: number }; weights: Array<{ name: string; values: number[] }> }>(
    "/api/train",
    { count, learningRate },
  );
}

export async function renderField(mode: RenderMode, selectedNeuron: number, timeTick: number): Promise<RenderPayload> {
  return await postJson<RenderPayload>("/api/render", {
    mode,
    selectedNeuron,
    timeTick,
  });
}
