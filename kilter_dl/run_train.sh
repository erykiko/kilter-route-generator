#!/usr/bin/env bash
set -euo pipefail

# Train model with full parameter set.
# Usage example:
#   ./kilter_dl/run_train.sh --device cuda --epochs 20 --run-name report_full

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PYTHON="$ROOT_DIR/kilter_dl/.venv/bin/python"

if [[ ! -x "$VENV_PYTHON" ]]; then
  echo "Error: venv python not found at $VENV_PYTHON"
  echo "Create venv first: python -m venv kilter_dl/.venv && source kilter_dl/.venv/bin/activate && pip install -r kilter_dl/requirements.txt"
  exit 1
fi

# Defaults (can be overridden by passing flags to this script)
DATASET="dataset/dataset/single_board.json"
TOKEN_MAP="dataset/token_to_id.json"
OUTPUT="kilter_dl/checkpoints/kilter_gen.pt"
BATCH_SIZE=32
EPOCHS=10
LR=0.0001
EMBED_DIM=128
HIDDEN_DIM=256
LAYERS=5
DROPOUT=0.15
VAL_RATIO=0.01
MAX_SAMPLES=0
DEVICE="cuda"
NUM_WORKERS=2
SEED=42
LOG_DIR="kilter_dl/logs"
RUN_NAME="report_full"

cd "$ROOT_DIR"

"$VENV_PYTHON" kilter_dl/train.py \
  --dataset "$DATASET" \
  --token-map "$TOKEN_MAP" \
  --output "$OUTPUT" \
  --batch-size "$BATCH_SIZE" \
  --epochs "$EPOCHS" \
  --lr "$LR" \
  --embed-dim "$EMBED_DIM" \
  --hidden-dim "$HIDDEN_DIM" \
  --layers "$LAYERS" \
  --dropout "$DROPOUT" \
  --val-ratio "$VAL_RATIO" \
  --max-samples "$MAX_SAMPLES" \
  --device "$DEVICE" \
  --num-workers "$NUM_WORKERS" \
  --seed "$SEED" \
  --log-dir "$LOG_DIR" \
  --run-name "$RUN_NAME" \
  "$@"
