#!/usr/bin/env bash
# One-time setup after cloning: Python env + board database + verify GUI assets.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$ROOT_DIR/kilter_dl/.venv"
REQUIREMENTS="$ROOT_DIR/kilter_dl/requirements.txt"
DB_PATH="$ROOT_DIR/boardlib_data/kilter.sqlite3"
CHECKPOINT="$ROOT_DIR/kilter_dl/checkpoints/kilter_gen.pt"

echo "==> [1/3] Python venv (kilter_dl/.venv)"
if [[ ! -x "$VENV_DIR/bin/python" ]]; then
  python -m venv "$VENV_DIR"
fi
"$VENV_DIR/bin/pip" install -q -r "$REQUIREMENTS"

echo "==> [2/3] Board database (boardlib_data/kilter.sqlite3)"
if [[ -f "$DB_PATH" ]]; then
  echo "    Already present, skipping download."
else
  SKIP_IMAGES=1 "$ROOT_DIR/boardlib_data/download_dataset_boards.sh"
fi

echo "==> [3/3] Checking other GUI assets"
missing=0
for path in \
  "$ROOT_DIR/dataset/token_to_id.json" \
  "$ROOT_DIR/dataset/id_to_token.json" \
  "$ROOT_DIR/dataset/dataset/single_board.json" \
  "$CHECKPOINT"; do
  if [[ ! -f "$path" ]]; then
    echo "    MISSING: $path" >&2
    missing=1
  fi
done
if (( missing )); then
  echo "Error: some tracked files are missing — run 'git pull' or restore them." >&2
  exit 1
fi

echo ""
echo "Setup complete. Build and run:"
echo "  ./kilter_cpp_gui/run.sh"
