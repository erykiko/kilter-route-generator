#!/usr/bin/env bash
set -euo pipefail

# Run trained model inference and log generated route in console.
# Usage example:
#   ./kilter_dl/run_generate.sh --layout-id 1270 --grade-id 1239 --max-len 64

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PYTHON="$ROOT_DIR/kilter_dl/.venv/bin/python"

if [[ ! -x "$VENV_PYTHON" ]]; then
  echo "Error: venv python not found at $VENV_PYTHON"
  echo "Create venv first: python -m venv kilter_dl/.venv && source kilter_dl/.venv/bin/activate && pip install -r kilter_dl/requirements.txt"
  exit 1
fi

# Defaults (can be overridden by passing flags to this script)
CHECKPOINT="kilter_dl/checkpoints/kilter_gen.pt"
TOKEN_MAP="dataset/token_to_id.json"
ID_MAP="dataset/id_to_token.json"
LAYOUT_ID=1272
GRADE_ID=1240
MAX_LEN=128
OUTPUT_JSON="kilter_dl/samples/generated_full_1270_1239.json"
CONSOLE_LOG="kilter_dl/logs/latest_generate_console.log"

cd "$ROOT_DIR"
mkdir -p "$(dirname "$OUTPUT_JSON")" "$(dirname "$CONSOLE_LOG")"

echo "Running generation with checkpoint: $CHECKPOINT"
echo "Logging console output to: $CONSOLE_LOG"

"$VENV_PYTHON" kilter_dl/generate.py \
  --checkpoint "$CHECKPOINT" \
  --token-map "$TOKEN_MAP" \
  --id-map "$ID_MAP" \
  --layout-id "$LAYOUT_ID" \
  --grade-id "$GRADE_ID" \
  --max-len "$MAX_LEN" \
  --output "$OUTPUT_JSON" \
  "$@" | tee "$CONSOLE_LOG"