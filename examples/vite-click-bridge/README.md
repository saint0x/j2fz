# Vite Click Bridge

This example shows the two halves of `j2fz` working together in one request path:

- the Node server uses `ts2fzy` to load and call a native Fzy shared library
- the Fzy shared library uses `fzy2ts` to call back into JavaScript through the host shim
- the Fzy side then applies one final native transform and returns the packed result back through `ts2fzy`

## Flow

1. The browser clicks `Run Bridge Click`.
2. The Node server increments a counter and calls the native export `bridge_click`.
3. The Fzy export opens `server/bridge-module.mjs` and awaits `doubleLater(count)`.
4. The Fzy export adds a final native `+10` lift after the JavaScript promise settles.
5. Node decodes the packed result and sends the full payload back to the browser as JSON.

## Run

```bash
cd /Users/deepsaint/Desktop/j2fz/examples/vite-click-bridge
npm install
npm run dev
```

The server auto-builds:

- `/Users/deepsaint/Desktop/j2fz/ts2fzy`
- `/Users/deepsaint/Desktop/j2fz/examples/vite-click-bridge/native`

Compiler selection order:

- `J2FZ_FZ_BIN`, if set
- `/Users/deepsaint/Desktop/fzyagent/.local/bin/fz`, if present
- `fz` on `PATH`
- a sibling checkout at `/Users/deepsaint/Desktop/fozzylang` via `cargo run -q -p fz -- ...`
