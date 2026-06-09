#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GUI_DIR="$SCRIPT_DIR"
BUILD_DIR="$GUI_DIR/build"
BINARY="$BUILD_DIR/kilter_gui"

usage() {
  cat <<EOF
Usage: $(basename "$0") [build|run|all]

  build   Configure and compile kilter_gui (default: all)
  run     Launch kilter_gui from repository root
  all     Build then run (default)

Examples:
  $(basename "$0")           # build + run
  $(basename "$0") build     # compile only
  $(basename "$0") run         # run only (binary must exist)
EOF
}

do_build() {
  echo "==> Configuring CMake in $BUILD_DIR"
  cmake -S "$GUI_DIR" -B "$BUILD_DIR"
  echo "==> Building kilter_gui"
  cmake --build "$BUILD_DIR" -j
  echo "==> Build OK: $BINARY"
}

do_run() {
  if [[ ! -x "$BINARY" ]]; then
    echo "Error: binary not found at $BINARY — run '$(basename "$0") build' first." >&2
    exit 1
  fi
  echo "==> Running from $REPO_ROOT"
  cd "$REPO_ROOT"
  exec "$BINARY" "$@"
}

cmd="${1:-all}"
shift || true

case "$cmd" in
  build)
    do_build
    ;;
  run)
    do_run "$@"
    ;;
  all)
    do_build
    do_run "$@"
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    echo "Unknown command: $cmd" >&2
    usage >&2
    exit 1
    ;;
esac
