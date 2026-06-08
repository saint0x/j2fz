#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
DEFAULT_BUNDLED_NODE="/Users/deepsaint/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node"
NODE_BIN="${J2FZ_NODE_BIN:-}"

if [[ -z "$NODE_BIN" ]]; then
  if [[ -x "$DEFAULT_BUNDLED_NODE" ]]; then
    NODE_BIN="$DEFAULT_BUNDLED_NODE"
  else
    NODE_BIN="node"
  fi
fi

exec "$NODE_BIN" \
  --import "$ROOT/examples/vite-neural-net-gpu/node_modules/tsx/dist/loader.mjs" \
  "$ROOT/examples/vite-neural-net-gpu/tests/native-smoke.ts"
