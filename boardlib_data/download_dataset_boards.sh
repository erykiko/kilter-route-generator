#!/usr/bin/env bash
set -euo pipefail

# Download board databases + images using boardlib
# only for boards present in this dataset.
#
# Dataset analysis shows all layouts belong to Kilter board,
# so this script downloads only: kilter.
#
# Optional sync username:
#   BOARDLIB_USERNAME="your_username" ./boardlib_data/download_dataset_boards.sh
#
# Optional runtime tuning:
#   RETRIES=6 RETRY_DELAY_SEC=8 ./boardlib_data/download_dataset_boards.sh

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PYTHON="$ROOT_DIR/kilter_dl/.venv/bin/python"
BOARDLIB_BIN="$ROOT_DIR/kilter_dl/.venv/bin/boardlib"
OUT_DIR="$ROOT_DIR/boardlib_data"
LOG_DIR="$OUT_DIR/logs"
RETRIES="${RETRIES:-5}"
RETRY_DELAY_SEC="${RETRY_DELAY_SEC:-10}"

if [[ ! -x "$VENV_PYTHON" ]]; then
  echo "Error: missing venv at $VENV_PYTHON"
  echo "Create it first and install requirements."
  exit 1
fi

"$VENV_PYTHON" -m pip install boardlib pillow >/dev/null

mkdir -p "$OUT_DIR" "$LOG_DIR"

BOARD="kilter"
DB_PATH="$OUT_DIR/${BOARD}.sqlite3"
IMG_DIR="$OUT_DIR/images_${BOARD}"
RUN_TS="$(date +%Y%m%d_%H%M%S)"
RUN_LOG="$LOG_DIR/${BOARD}_download_${RUN_TS}.log"
FAIL_LOG="$LOG_DIR/${BOARD}_download_failures_${RUN_TS}.log"

run_with_retry() {
  local label="$1"
  shift
  local attempt=1
  while (( attempt <= RETRIES )); do
    echo "[$label] Attempt $attempt/$RETRIES" | tee -a "$RUN_LOG"
    if "$@" 2>&1 | tee -a "$RUN_LOG"; then
      echo "[$label] Success" | tee -a "$RUN_LOG"
      return 0
    fi
    if (( attempt < RETRIES )); then
      echo "[$label] Failed. Retrying in ${RETRY_DELAY_SEC}s..." | tee -a "$RUN_LOG"
      sleep "$RETRY_DELAY_SEC"
    fi
    attempt=$((attempt + 1))
  done
  return 1
}

echo "=== Downloading database for: $BOARD ==="
if [[ -f "$DB_PATH" ]]; then
  echo "Database already exists, skipping download: $DB_PATH" | tee -a "$RUN_LOG"
else
  if [[ -n "${BOARDLIB_USERNAME:-}" ]]; then
    run_with_retry "database" "$BOARDLIB_BIN" database "$BOARD" "$DB_PATH" --username "$BOARDLIB_USERNAME" || {
      echo "Database download failed after retries. See: $RUN_LOG" | tee -a "$FAIL_LOG"
      exit 1
    }
  else
    run_with_retry "database" "$BOARDLIB_BIN" database "$BOARD" "$DB_PATH" || {
      echo "Database download failed after retries. See: $RUN_LOG" | tee -a "$FAIL_LOG"
      exit 1
    }
  fi
fi

if [[ "${SKIP_IMAGES:-0}" == "1" ]]; then
  echo "=== Skipping images (SKIP_IMAGES=1; GUI needs only the database) ===" | tee -a "$RUN_LOG"
else
  echo "=== Downloading images for: $BOARD ==="
  run_with_retry "images" "$BOARDLIB_BIN" images "$BOARD" "$DB_PATH" "$IMG_DIR" --composite || {
    {
      echo "Image download failed after retries."
      echo "Database: $DB_PATH"
      echo "Image dir: $IMG_DIR"
      echo "Run log:  $RUN_LOG"
    } | tee -a "$FAIL_LOG"
    exit 1
  }
fi

echo "Done."
echo "Database: $DB_PATH"
if [[ "${SKIP_IMAGES:-0}" != "1" ]]; then
  echo "Images:   $IMG_DIR"
fi
echo "Run log:  $RUN_LOG"
