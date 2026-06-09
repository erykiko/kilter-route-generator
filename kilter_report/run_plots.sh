#!/usr/bin/env bash
set -euo pipefail

# Bootstrap izolowanego venv biblioteki kilter_report i wygeneruj wykresy
# do report/figures/ na podstawie logów eksperymentów kilter_dl.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIB_DIR="$ROOT_DIR/kilter_report"
VENV_DIR="$LIB_DIR/.venv"
VENV_PY="$VENV_DIR/bin/python"

if [[ ! -x "$VENV_PY" ]]; then
  echo "[kilter_report] Tworzę izolowane venv: $VENV_DIR"
  python3 -m venv "$VENV_DIR"
  "$VENV_PY" -m pip install --upgrade pip wheel >/dev/null
  "$VENV_PY" -m pip install -r "$LIB_DIR/requirements.txt"
fi

cd "$ROOT_DIR"
exec "$VENV_PY" -m kilter_report \
  --logs-dir kilter_dl/logs \
  --samples-dir kilter_dl/samples \
  --output-dir report/figures \
  "$@"
