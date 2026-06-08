import {
  addPoint,
  clearPoints,
  fetchBootstrap,
  randomizeModel,
  renderField,
  trainModel,
} from "./bridge";
import { paintField } from "./renderer";
import type { BootstrapPayload, LessonStep, ModelStats, PointRecord, RenderMode, WeightGroup } from "./state";
import { highlightSource, renderStepRail } from "./walkthrough";

interface AppState {
  bootstrap: BootstrapPayload;
  mode: RenderMode;
  selectedNeuron: number;
  selectedClass: number;
  learningRate: number;
  timeTick: number;
  points: PointRecord[];
  weights: WeightGroup[];
  stats: ModelStats;
  activeStepId: string;
  activeSourceTab: string;
  autoTraining: boolean;
}

function formatPercent(value: number): string {
  return `${Math.round(value * 100)}%`;
}

function formatLoss(value: number): string {
  return value.toFixed(3);
}

function normalizeCanvasCoordinate(position: number, size: number): number {
  return ((position / size) * 2) - 1;
}

export async function mountApp(root: HTMLElement): Promise<void> {
  const bootstrap = await fetchBootstrap();
  const initialStep = bootstrap.steps[0]?.id ?? "inputs";
  const state: AppState = {
    bootstrap,
    mode: "class",
    selectedNeuron: 0,
    selectedClass: 0,
    learningRate: 0.04,
    timeTick: 0,
    points: bootstrap.points,
    weights: bootstrap.weights,
    stats: bootstrap.stats,
    activeStepId: initialStep,
    activeSourceTab: "native/src/lib.fzy",
    autoTraining: false,
  };

  root.innerHTML = `
    <div class="page">
      <section class="hero">
        <div>
          <p class="eyebrow">Vite + j2fz + Fzy GPU + SIMD</p>
          <h1>Neural Ink Garden</h1>
          <p class="lede">Teach a tiny 2→8→3 neural net with clicks, then watch Fzy render its decision field in parallel through a real GPU kernel.</p>
        </div>
        <div class="hero-stats">
          <div><span>Loss</span><strong id="loss-value">0.000</strong></div>
          <div><span>Accuracy</span><strong id="accuracy-value">0%</strong></div>
          <div><span>Train Step</span><strong id="step-value">0</strong></div>
          <div><span>Points</span><strong id="point-count-value">0</strong></div>
        </div>
      </section>
      <section class="workspace">
        <div class="canvas-column">
          <div class="toolbar">
            <label>Class
              <select id="class-select"></select>
            </label>
            <label>Render Mode
              <select id="mode-select">
                <option value="class">Smooth Field</option>
                <option value="ascii">ASCII Field</option>
                <option value="neuron">Neuron Heat</option>
              </select>
            </label>
            <label>Neuron
              <input id="neuron-select" type="range" min="0" max="7" step="1" value="0" />
              <span id="neuron-value">0</span>
            </label>
            <label>Learning Rate
              <input id="learning-rate" type="range" min="0.005" max="0.12" step="0.001" value="0.04" />
              <span id="learning-rate-value">0.040</span>
            </label>
          </div>
          <canvas id="field-canvas" width="${bootstrap.width}" height="${bootstrap.height}"></canvas>
          <div class="actions">
            <button id="train-toggle" type="button">Train</button>
            <button id="single-step" type="button">Single Step</button>
            <button id="reset-model" type="button">Reset Model</button>
            <button id="clear-points" type="button">Clear Points</button>
          </div>
          <p id="status-line" class="status-line">Click the canvas to add labeled points. Then train the network and inspect how the field bends.</p>
        </div>
        <aside class="side-column">
          <div class="panel">
            <h2>Lesson Rail</h2>
            <div id="lesson-rail" class="lesson-rail"></div>
            <div id="lesson-body" class="lesson-body"></div>
          </div>
          <div class="panel">
            <h2>Weights</h2>
            <div id="weight-groups" class="weight-groups"></div>
          </div>
          <div class="panel">
            <h2>Code Panel</h2>
            <div id="source-tabs" class="source-tabs"></div>
            <pre id="source-panel" class="source-panel"></pre>
          </div>
        </aside>
      </section>
    </div>
  `;

  const canvas = root.querySelector<HTMLCanvasElement>("#field-canvas");
  const classSelect = root.querySelector<HTMLSelectElement>("#class-select");
  const modeSelect = root.querySelector<HTMLSelectElement>("#mode-select");
  const neuronSelect = root.querySelector<HTMLInputElement>("#neuron-select");
  const neuronValue = root.querySelector<HTMLSpanElement>("#neuron-value");
  const learningRate = root.querySelector<HTMLInputElement>("#learning-rate");
  const learningRateValue = root.querySelector<HTMLSpanElement>("#learning-rate-value");
  const trainToggle = root.querySelector<HTMLButtonElement>("#train-toggle");
  const singleStep = root.querySelector<HTMLButtonElement>("#single-step");
  const resetModel = root.querySelector<HTMLButtonElement>("#reset-model");
  const clearPointsButton = root.querySelector<HTMLButtonElement>("#clear-points");
  const lessonRail = root.querySelector<HTMLDivElement>("#lesson-rail");
  const lessonBody = root.querySelector<HTMLDivElement>("#lesson-body");
  const weightGroups = root.querySelector<HTMLDivElement>("#weight-groups");
  const sourceTabs = root.querySelector<HTMLDivElement>("#source-tabs");
  const sourcePanel = root.querySelector<HTMLPreElement>("#source-panel");
  const statusLine = root.querySelector<HTMLParagraphElement>("#status-line");
  const lossValue = root.querySelector<HTMLElement>("#loss-value");
  const accuracyValue = root.querySelector<HTMLElement>("#accuracy-value");
  const stepValue = root.querySelector<HTMLElement>("#step-value");
  const pointCountValue = root.querySelector<HTMLElement>("#point-count-value");

  if (
    !canvas ||
    !classSelect ||
    !modeSelect ||
    !neuronSelect ||
    !neuronValue ||
    !learningRate ||
    !learningRateValue ||
    !trainToggle ||
    !singleStep ||
    !resetModel ||
    !clearPointsButton ||
    !lessonRail ||
    !lessonBody ||
    !weightGroups ||
    !sourceTabs ||
    !sourcePanel ||
    !statusLine ||
    !lossValue ||
    !accuracyValue ||
    !stepValue ||
    !pointCountValue
  ) {
    throw new Error("app UI failed to initialize");
  }

  for (const option of bootstrap.classOptions) {
    const element = document.createElement("option");
    element.value = String(option.id);
    element.textContent = option.label;
    classSelect.appendChild(element);
  }

  function activeStep(): LessonStep {
    return bootstrap.steps.find((step) => step.id === state.activeStepId) ?? bootstrap.steps[0]!;
  }

  function renderWeights(): void {
    weightGroups.innerHTML = state.weights
      .map((group) => {
        const items = group.values
          .map((value, index) => `<li><span>${index}</span><strong>${value.toFixed(3)}</strong></li>`)
          .join("");
        return `<section class="weight-group"><h3>${group.name}</h3><ul>${items}</ul></section>`;
      })
      .join("");
  }

  function renderSources(): void {
    sourceTabs.innerHTML = "";
    for (const tabName of Object.keys(bootstrap.sourceTabs)) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = tabName === state.activeSourceTab ? "source-tab active" : "source-tab";
      button.textContent = tabName;
      button.addEventListener("click", () => {
        state.activeSourceTab = tabName;
        renderSources();
      });
      sourceTabs.appendChild(button);
    }
    sourcePanel.innerHTML = highlightSource(
      bootstrap.sourceTabs[state.activeSourceTab] ?? "",
      activeStep().focus,
    );
  }

  function renderLesson(): void {
    renderStepRail(lessonRail, bootstrap.steps, state.activeStepId, (stepId) => {
      state.activeStepId = stepId;
      renderLesson();
      renderSources();
    });
    const step = activeStep();
    lessonBody.innerHTML = `<h3>${step.title}</h3><p>${step.body}</p><p class="focus">Focus: ${step.focus.join(", ")}</p>`;
  }

  function renderStats(): void {
    lossValue.textContent = formatLoss(state.stats.loss);
    accuracyValue.textContent = formatPercent(state.stats.accuracy);
    stepValue.textContent = String(state.stats.step);
    pointCountValue.textContent = String(state.stats.pointCount);
  }

  async function drawFrame(): Promise<void> {
    const frame = await renderField(state.mode, state.selectedNeuron, state.timeTick);
    state.points = frame.points;
    state.stats = frame.stats;
    state.weights = frame.weights;
    paintField(
      canvas,
      frame.frameBase64,
      frame.width,
      frame.height,
      frame.points,
      bootstrap.classOptions.map((entry) => entry.hex),
      state.mode,
    );
    renderWeights();
    renderStats();
  }

  async function trainBatch(count: number): Promise<void> {
    const result = await trainModel(count, state.learningRate);
    state.stats = result.stats;
    state.weights = result.weights;
    await drawFrame();
  }

  async function refreshAll(message?: string): Promise<void> {
    if (message) {
      statusLine.textContent = message;
    }
    await drawFrame();
  }

  let trainingLoopHandle: number | null = null;

  function stopTrainingLoop(): void {
    if (trainingLoopHandle !== null) {
      cancelAnimationFrame(trainingLoopHandle);
      trainingLoopHandle = null;
    }
    state.autoTraining = false;
    trainToggle.textContent = "Train";
  }

  async function runTrainingLoop(): Promise<void> {
    if (!state.autoTraining) {
      return;
    }
    state.timeTick += 1;
    await trainBatch(8);
    trainingLoopHandle = requestAnimationFrame(() => {
      void runTrainingLoop();
    });
  }

  canvas.addEventListener("click", async (event) => {
    const rect = canvas.getBoundingClientRect();
    const x = normalizeCanvasCoordinate(event.clientX - rect.left, rect.width);
    const y = normalizeCanvasCoordinate(event.clientY - rect.top, rect.height);
    const result = await addPoint(x, y, state.selectedClass);
    state.points = result.points;
    state.stats = result.stats;
    await refreshAll(`Added ${bootstrap.classOptions[state.selectedClass]?.label ?? "point"} at (${x.toFixed(2)}, ${y.toFixed(2)}).`);
  });

  classSelect.addEventListener("change", () => {
    state.selectedClass = Number(classSelect.value);
    statusLine.textContent = `Painting ${bootstrap.classOptions[state.selectedClass]?.label ?? "class"} examples.`;
  });

  modeSelect.addEventListener("change", async () => {
    state.mode = modeSelect.value as RenderMode;
    await refreshAll(`Switched render mode to ${state.mode}.`);
  });

  neuronSelect.addEventListener("input", async () => {
    state.selectedNeuron = Number(neuronSelect.value);
    neuronValue.textContent = String(state.selectedNeuron);
    if (state.mode === "neuron") {
      await refreshAll(`Inspecting hidden neuron ${state.selectedNeuron}.`);
    }
  });

  learningRate.addEventListener("input", () => {
    state.learningRate = Number(learningRate.value);
    learningRateValue.textContent = state.learningRate.toFixed(3);
  });

  trainToggle.addEventListener("click", async () => {
    if (state.autoTraining) {
      stopTrainingLoop();
      statusLine.textContent = "Training paused.";
      return;
    }
    state.autoTraining = true;
    trainToggle.textContent = "Pause";
    statusLine.textContent = "Training in motion. The browser is driving the steps while Fzy updates the model.";
    await runTrainingLoop();
  });

  singleStep.addEventListener("click", async () => {
    stopTrainingLoop();
    state.timeTick += 1;
    await trainBatch(1);
    statusLine.textContent = "Ran one exact training step.";
  });

  resetModel.addEventListener("click", async () => {
    stopTrainingLoop();
    const result = await randomizeModel(Date.now() & 0x7fffffff);
    state.stats = result.stats;
    state.weights = result.weights;
    await refreshAll("Reset the model weights. The field is random again.");
  });

  clearPointsButton.addEventListener("click", async () => {
    stopTrainingLoop();
    const result = await clearPoints();
    state.points = result.points;
    state.stats = result.stats;
    await refreshAll("Cleared all training points.");
  });

  renderLesson();
  renderSources();
  renderWeights();
  renderStats();
  await refreshAll("Click to plant ink species, then train the network and inspect the field.");
}
