# Neural Ink Garden

Neural Ink Garden is an embedded Vite + j2fz demo where the browser teaches a small `2 → 8 → 3` neural network and the native Fzy side trains and renders the learned field.

## What This Shows

- a real manually trained MLP on the Fzy side
- GPU field evaluation through Fzy `core.gpu`
- SIMD-assisted hidden-layer math and aggregation through Fzy `core.simd`
- a Vite/TypeScript lesson UI that lets you place points, train, inspect neurons, and read the source live

## Flow

1. The browser sends clicks and controls to the Node server.
2. The Node server keeps the current point set and model buffers.
3. `ts2fzy` loads the native Fzy library and calls:
   - `randomize_model`
   - `evaluate_model`
   - `train_steps`
   - `render_field`
4. Fzy trains on CPU, renders the full field through a GPU kernel, and writes frame pixels back into JS-owned buffers.
   If the local GPU path produces an all-zero frame, the native side falls back to the host SIMD path so the demo stays usable while preserving the same model behavior.
5. The browser paints the frame and overlays points, ASCII, walkthrough text, weight groups, and code excerpts.

## Run

```bash
cd /Users/deepsaint/Desktop/j2fz/examples/vite-neural-net-gpu
npm install
npm run dev
```

The server auto-builds:

- `/Users/deepsaint/Desktop/j2fz/ts2fzy`
- `/Users/deepsaint/Desktop/j2fz/examples/vite-neural-net-gpu/native`

Compiler selection order:

- `J2FZ_FZ_BIN`, if set
- a sibling checkout at `/Users/deepsaint/Desktop/fozzylang` via `cargo run -q -p fz -- ...`
- `/Users/deepsaint/Desktop/fzyagent/.local/bin/fz`, if present
- `fz` on `PATH`
