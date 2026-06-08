Absolutely. Build this:

Neural Ink Garden

A tiny neural net learns to draw living ink patterns in real time. You click points on an HTML canvas, assign them “vibes”/classes, and a FZY GPU kernel renders the learned decision field as animated ASCII/visual ink.

spec.md — Neural Ink Garden

1. Concept

Neural Ink Garden is an interactive neural-network learning demo built with FZY GPU kernels, Metal backend execution, and a TypeScript/HTML visualization layer through j2fz.

The user teaches a tiny neural net by placing points on a 2D canvas. Each point belongs to one of several “ink species.” The neural net learns the boundary between these species. The GPU renders the learned field in real time as animated ink, ASCII, or color cells.

This project is both:

1. A creative neural-net toy.
2. A walkthrough page for learning:
    * what inputs are
    * what weights are
    * what activations are
    * what loss is
    * what backpropagation is
    * how FZY expresses GPU kernels
    * how JS/TS talks to FZY through j2fz

The core demo:

click points → train tiny net → GPU renders decision field → user watches learning happen

2. Why This Is The Right Size

This is intentionally small.

It does not require:

* large datasets
* tensor libraries
* matrix-multiplication engine
* full autograd
* model serialization
* complicated UI
* big training infra

It only needs:

* a tiny MLP
* a 2D canvas/grid
* a GPU forward-pass kernel
* a CPU or GPU training loop
* a JS visualization shell
* a step-by-step educational walkthrough

3. Final User Experience

The page opens with a dark canvas.

The user sees:

Step 1: Place training points

They click several points and assign each one a class:

* red ink
* blue ink
* gold ink

Then they click:

Train

The visualization begins animating.

The canvas fills with a living neural field showing what the model believes every coordinate belongs to.

The page has a side panel showing:

Input: x, y
Layer 1: hidden neurons
Activation: tanh
Output: class score
Loss: how wrong the net is

The user can click through the walkthrough:

1. Coordinates become inputs
2. Weights bend space
3. Activations create curves
4. Loss measures wrongness
5. Training changes weights
6. GPU renders every pixel in parallel
7. FZY kernel code runs through Metal
8. j2fz bridges JS controls to FZY runtime

4. Project Name

Recommended name:

neural_ink_garden

Example path:

examples/neural_ink_garden

5. Core Learning Goal

The project should teach this idea:

A neural network is not magic. It is a small mathematical machine that bends an input space until different examples fall into different regions.

The user should visually understand that:

* each pixel is an input
* the network runs on every pixel
* the output is a class/color
* training reshapes the field
* GPU kernels are perfect for evaluating thousands of pixels at once

6. Architecture

HTML/TS UI
  ↓
j2fz bridge
  ↓
FZY host runtime
  ↓
FZY-authored GPU kernel
  ↓
Metal backend
  ↓
GPU field buffer
  ↓
JS visualization layer

7. Directory Structure

examples/neural_ink_garden/
  README.md
  spec.md
  fozzy.toml
  src/
    main.fzy
    kernels/
      forward_field.fzy
      train_step.fzy
      loss_field.fzy
    bridge/
      j2fz_api.fzy
  web/
    index.html
    src/
      main.ts
      app.ts
      bridge.ts
      renderer.ts
      walkthrough.ts
      state.ts
      styles.css
    package.json
    vite.config.ts
  artifacts/
    traces/
    screenshots/

8. Neural Net Shape

Use a tiny MLP:

2 inputs → 8 hidden neurons → 3 outputs

Inputs:

x, y

Hidden layer:

h = tanh(W1 * input + b1)

Output layer:

logits = W2 * h + b2

Prediction:

class = argmax(logits)

Initial supported classes:

0 = red ink
1 = blue ink
2 = gold ink

9. Model Parameters

For the MVP, store parameters in flat buffers.

W1: 2 x 8
b1: 8
W2: 8 x 3
b2: 3

Total parameters:

16 + 8 + 24 + 3 = 51 parameters

This is small enough to inspect visually in the UI.

10. GPU Kernel: Forward Field

Each GPU thread owns one output pixel/cell.

For a canvas of width x height, each thread:

1. Convert pixel index to normalized coordinate x,y
2. Run tiny neural net forward pass
3. Choose class
4. Write output color/intensity/class into field buffer

Pseudo-FZY:

kernel forward_field(
  params: ModelParams,
  out: mut FieldBuffer,
  width: i32,
  height: i32,
  time: f32
) {
  let id = gpu.thread_id()
  let px = id % width
  let py = id / width
  let x = normalize_x(px, width)
  let y = normalize_y(py, height)
  let h0 = tanh(x * w1_00 + y * w1_10 + b1_0)
  ...
  let logit0 = dot(hidden, w2_class0) + b2_0
  let logit1 = dot(hidden, w2_class1) + b2_1
  let logit2 = dot(hidden, w2_class2) + b2_2
  let cls = argmax3(logit0, logit1, logit2)
  out[id] = encode_class(cls, confidence, time)
}

11. Training Loop

MVP training can start on CPU/FZY host first.

Training data is tiny:

max_points = 256

Each point:

x: f32
y: f32
class_id: i32

Training step:

for point in training_points:
  forward pass
  compute softmax loss
  compute gradients manually
  update weights

Later upgrade:

GPU train_step kernel

But the MVP only requires GPU inference/rendering.

12. Loss Function

Use softmax cross-entropy.

For teaching, expose simplified UI language:

Loss = how surprised the model is by the correct answer.

Display:

current_loss: 0.842
accuracy: 72%
training_step: 134

13. Visualization Modes

The UI should support three modes.

13.1 Smooth Field

Canvas color per pixel/class.

red area = class 0
blue area = class 1
gold area = class 2

13.2 ASCII Field

Use ASCII chars by confidence.

 .:-=+*#%@

Class affects glyph family.

Example:

red = @ # %
blue = ~ = -
gold = * + .

13.3 Neuron Heat Mode

Click a hidden neuron and view its activation over the full input field.

This is the most educational mode.

It shows that each neuron carves space in a specific way.

14. HTML Walkthrough

The page should have a clickable lesson rail.

Step 1 — Inputs

Teach:

Every pixel becomes two numbers: x and y.

Show:

pixel → normalized coordinate → neural input

Step 2 — Weights

Teach:

Weights decide which directions in space matter.

Show editable weights or animated sliders.

Step 3 — Hidden Neurons

Teach:

Each hidden neuron makes a soft boundary.

Allow user to inspect hidden neuron 0–7.

Step 4 — Activations

Teach:

tanh bends straight math into curved regions.

Toggle:

linear vs tanh

Step 5 — Outputs

Teach:

The output neurons compete.

Show:

red_score
blue_score
gold_score

Step 6 — Loss

Teach:

Loss tells the model how wrong it is.

Show loss changing while training.

Step 7 — Training

Teach:

Training nudges weights so correct regions grow around examples.

Animate point influence.

Step 8 — GPU Kernel

Teach:

The GPU runs the same neural net for every pixel at once.

Show FZY kernel snippet side-by-side with visual output.

Step 9 — j2fz Bridge

Teach:

The browser controls the model, while FZY owns the compute path.

Show:

JS click event → j2fz call → FZY buffer update → GPU render

15. j2fz API Contract

Expose these calls to TypeScript:

type Point = {
  x: number
  y: number
  classId: number
}
type ModelStats = {
  loss: number
  accuracy: number
  step: number
}
type FieldFrame = {
  width: number
  height: number
  data: Uint8Array
}

Required bridge methods:

initGarden(width: number, height: number): void
addPoint(x: number, y: number, classId: number): void
clearPoints(): void
randomizeModel(): void
trainSteps(count: number, learningRate: number): ModelStats
renderField(mode: "class" | "ascii" | "neuron", selectedNeuron?: number): FieldFrame
getModelStats(): ModelStats
getWeights(): Float32Array
setLearningRate(value: number): void

16. TypeScript UI Components

App
  CanvasRenderer
  PointToolbar
  TrainingControls
  WalkthroughPanel
  KernelCodePanel
  WeightInspector
  StatsPanel
  ModeSwitcher

17. Controls

Required controls:

Class selector: Red / Blue / Gold
Train / Pause button
Single Step button
Reset Model button
Clear Points button
Learning Rate slider
Render Mode selector
Neuron selector 0–7
ASCII toggle

18. Educational Code Panel

The UI should show live snippets from the FZY source.

Example tabs:

forward_field.fzy
train_step.fzy
j2fz_api.fzy
main.ts

When the walkthrough reaches “GPU Kernel,” highlight:

let id = gpu.thread_id()

When it reaches “Activation,” highlight:

let h = tanh(z)

When it reaches “Output,” highlight:

let cls = argmax3(logit0, logit1, logit2)

19. MVP Build Phases

Phase 1 — Static Render

Goal:

Render a neural field from random weights.

Tasks:

* create example folder
* define model param buffer
* write GPU forward kernel
* render field to CPU buffer
* display in terminal or HTML canvas

Phase 2 — JS Bridge

Goal:

HTML page can request frames from FZY.

Tasks:

* add j2fz bridge methods
* call renderField
* draw to canvas
* add mode switching

Phase 3 — Training Points

Goal:

User can click points onto canvas.

Tasks:

* add point state in TS
* send points to FZY
* render point overlays
* support class selection

Phase 4 — CPU Training

Goal:

Tiny neural net learns clicked points.

Tasks:

* implement forward pass on host
* implement manual gradients
* update weights
* expose loss/accuracy

Phase 5 — Live Walkthrough

Goal:

User can click through lesson steps.

Tasks:

* add walkthrough panel
* add code snippet highlights
* add explanatory text
* sync visual mode to current lesson

Phase 6 — GPU Training Upgrade

Goal:

Move train step into FZY GPU kernel.

Optional for MVP.

20. Acceptance Criteria

The demo is successful when:

* user can open HTML page
* user can place colored points
* model can train on those points
* decision field visibly changes
* FZY GPU kernel renders the field
* walkthrough explains neural-net concepts
* walkthrough explains FZY-specific compute flow
* j2fz bridge is used for browser/runtime interaction
* deterministic run mode can record/replay at least one training session

21. Recommended Example Script

Demo flow:

1. Open page.
2. Add three red points in upper left.
3. Add three blue points in lower right.
4. Add three gold points in center.
5. Click Train.
6. Watch regions grow.
7. Switch to Hidden Neuron 3.
8. Show how one neuron activates over space.
9. Switch to ASCII mode.
10. Open FZY code panel.
11. Show that every pixel is running the same neural net on GPU.

22. Stretch Ideas

22.1 Draw A Shape

User draws a rough shape.

The model learns the inside/outside boundary.

22.2 Audio-Reactive Learning

Microphone/audio amplitude changes the field time parameter.

22.3 Neural Fashion Print Generator

Export the learned ink field as a textile pattern.

22.4 Corpus Mode

Use source-code tokens as training points.

Different classes become:

functions
types
comments
imports

The neural field becomes a living map of a codebase.

22.5 Shader Mode

Instead of class colors, output procedural values:

x, y, time → brightness

This turns the MLP into a neural shader.

23. Why This Project Matters

This project proves that FZY can express more than normal systems code.

It proves FZY can be used for:

* GPU-native creative computing
* educational neural-net demos
* real-time visualization
* browser/runtime interop
* deterministic replayable compute experiments
* small ML systems without PyTorch/TensorFlow

The important idea:

FZY is not just calling a neural net.
FZY is describing the neural net as compute.

24. MVP Summary

Build:

examples/neural_ink_garden

A browser-based neural-net playground where:

clicked points train a tiny MLP
FZY GPU kernels render the learned field
j2fz connects TS controls to FZY compute
the page teaches neural nets and FZY at the same time

The final output should feel like:

a neural-net museum exhibit
a GPU kernel demo
a tiny creative coding instrument
and a FZY language showcase