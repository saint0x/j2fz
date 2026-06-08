# j2fz

`j2fz` is a bidirectional Fzy and JavaScript interop workspace.

It now has two explicit product roots:

- [ts2fzy](ts2fzy)
  TypeScript and JavaScript calling native [Fzy](https://github.com/saint0x/fzy) libraries through the Fzy C ABI manifest.
- [fzy2ts](fzy2ts)
  Fzy calling into a JavaScript host through an explicit host ABI and Fzy-side interop module.

## Direction Split

`ts2fzy/` is the JS-hosted side:

- load Fzy native libraries
- generate TypeScript bindings
- register JS callbacks that Fzy can invoke
- handle ownership, async exports, and raw unsafe escape hatches

`fzy2ts/` is the Fzy-hosted side:

- declare the JavaScript host ABI explicitly in idiomatic Fzy
- verify a production-safe imported host ABI surface under strict Fzy rules
- give Fzy code a stable handle-based surface for JS modules, exports, values, calls, and release
- keep the reverse direction explicit instead of treating JavaScript like magic native syntax

`ts2fzy/` also now contains the reverse-direction JS host runtime state machine:

- module loading and export resolution
- handle-based JS value storage
- explicit call and await lifecycle
- explicit callback registration lifecycle
- a native host shim plus sidecar-process seam that exports the ABI symbols `fzy2ts` imports

## When This Workspace Is Useful

- fullstack systems where Node owns the app shell and Fzy owns hot native modules
- desktop or Electron apps where TypeScript owns UI and Fzy owns local capability
- plugin systems that need both JS -> Fzy and Fzy -> JS integration points
- agent, automation, or tool runtimes that need native execution plus JS ecosystem access
- host applications embedding Fzy but still wanting selective access to JavaScript libraries

## Repo Layout

- [ts2fzy](ts2fzy)
- [fzy2ts](fzy2ts)
- [examples/vite-click-bridge](examples/vite-click-bridge)
  A one-page Vite demo where Node calls a Fzy shared library through `ts2fzy`, and the Fzy side calls back into a real JavaScript module through `fzy2ts`.
- [examples/vite-neural-net-gpu](examples/vite-neural-net-gpu)
  A Vite + j2fz demo where a tiny native Fzy neural net trains on clicked points and renders its learned field through the GPU path.

Each direction is intentionally separate so the host model, docs, tests, and future packaging can evolve without collapsing back into a one-sided bridge.
