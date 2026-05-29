#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT}/dist/native"
SRC="${ROOT}/native/j2fz_js_host_shim.c"

mkdir -p "${OUT_DIR}"

case "$(uname -s)" in
  Darwin)
    OUT_FILE="${OUT_DIR}/libj2fz_host_shim.dylib"
    ;;
  Linux)
    OUT_FILE="${OUT_DIR}/libj2fz_host_shim.so"
    ;;
  *)
    echo "unsupported host OS: $(uname -s)" >&2
    exit 1
    ;;
esac

clang -shared -fPIC -O2 -Wall -Wextra -o "${OUT_FILE}" "${SRC}" -lpthread
echo "${OUT_FILE}"

