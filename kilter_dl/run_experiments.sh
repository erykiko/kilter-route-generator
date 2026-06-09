#!/usr/bin/env bash
set -euo pipefail

# =====================================================================
# Scenariusze eksperymentów dla projektu kilter_dl.
#
# Skrypt uruchamia dwie grupy eksperymentów:
#   A) TRENING  – pokazuje wpływ parametrów uczenia (lr, dropout,
#                 rozmiar modelu, batch_size, liczba warstw, embed_dim)
#                 na jakość modelu (val_nll, val_ppl).
#   B) GENERACJA – pokazuje wpływ parametrów dekodowania (greedy vs
#                  sampling, temperature, top-k, top-p, repetition
#                  penalty) na te same warunki wejściowe (layout, grade).
#
# Każdy eksperyment trenuje/zapisuje wyniki do osobnego katalogu, więc
# po zakończeniu można porównać metryki i wygenerowane drogi.
#
# Tryby:
#   FAST=1 (domyślnie) – ograniczony zbiór i mała liczba epok – szybko,
#                        tylko do pokazania trendów.
#   FAST=0             – pełny zbiór i więcej epok – „raportowy” bieg.
#
# Przykład wywołania:
#   ./kilter_dl/run_experiments.sh                 # tryb FAST
#   FAST=0 ./kilter_dl/run_experiments.sh          # pełen tryb
#   ./kilter_dl/run_experiments.sh --only-generate # bez treningów
#   ./kilter_dl/run_experiments.sh --only-train    # bez generacji
#   GEN_CHECKPOINT=kilter_dl/checkpoints/kilter_gen.pt \
#       ./kilter_dl/run_experiments.sh --only-generate
#
# Wyniki:
#   kilter_dl/logs/exp_*/                 – logi treningu (CSV/JSONL)
#   kilter_dl/checkpoints/exp_*.pt        – checkpointy modeli
#   kilter_dl/samples/exp_*/              – wygenerowane drogi (JSON + .log)
# =====================================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_PYTHON="$ROOT_DIR/kilter_dl/.venv/bin/python"

if [[ ! -x "$VENV_PYTHON" ]]; then
  echo "Error: venv python not found at $VENV_PYTHON"
  echo "Create venv first: python -m venv kilter_dl/.venv && source kilter_dl/.venv/bin/activate && pip install -r kilter_dl/requirements.txt"
  exit 1
fi

cd "$ROOT_DIR"

# --------- Parsowanie argumentów ---------
ONLY_TRAIN=0
ONLY_GENERATE=0
for arg in "$@"; do
  case "$arg" in
    --only-train)    ONLY_TRAIN=1 ;;
    --only-generate) ONLY_GENERATE=1 ;;
    -h|--help)
      sed -n '1,40p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg"; exit 1 ;;
  esac
done

# --------- Konfiguracja globalna ---------
FAST="${FAST:-1}"
DEVICE="${DEVICE:-cuda}"
SEED="${SEED:-42}"
NUM_WORKERS="${NUM_WORKERS:-2}"

DATASET="dataset/dataset/single_board.json"
TOKEN_MAP="dataset/token_to_id.json"
ID_MAP="dataset/id_to_token.json"

LOG_ROOT="kilter_dl/logs"
CKPT_ROOT="kilter_dl/checkpoints"
SAMPLES_ROOT="kilter_dl/samples"

mkdir -p "$LOG_ROOT" "$CKPT_ROOT" "$SAMPLES_ROOT"

if [[ "$FAST" == "1" ]]; then
  MAX_SAMPLES=20000   # mały podzbiór – szybkie eksperymenty pokazujące trendy
  EPOCHS=5
  VAL_RATIO=0.05
else
  MAX_SAMPLES=0       # pełny zbiór
  EPOCHS=30
  VAL_RATIO=0.01
fi

echo "============================================================"
echo " kilter_dl experiments  |  FAST=$FAST  |  DEVICE=$DEVICE"
echo "  EPOCHS=$EPOCHS  MAX_SAMPLES=$MAX_SAMPLES  VAL_RATIO=$VAL_RATIO"
echo "============================================================"

# Domyślny prompt do generacji (te same id co w run_generate.sh).
LAYOUT_ID="${LAYOUT_ID:-1272}"   # 12x12 without kickboard Square
GRADE_ID="${GRADE_ID:-1240}"     # 6a+/V3

# =====================================================================
# Pomocnicze funkcje
# =====================================================================

# Trening pojedynczego eksperymentu.
# Argumenty:
#   $1 nazwa eksperymentu (run_name + nazwa checkpointa)
#   pozostałe argumenty – flagi przekazywane do train.py.
run_train_experiment() {
  local name="$1"; shift
  local ckpt="$CKPT_ROOT/exp_${name}.pt"
  echo
  echo "------------------------------------------------------------"
  echo "[A] TRAIN  exp=$name"
  echo "    extra args: $*"
  echo "------------------------------------------------------------"
  "$VENV_PYTHON" kilter_dl/train.py \
    --dataset "$DATASET" \
    --token-map "$TOKEN_MAP" \
    --output "$ckpt" \
    --val-ratio "$VAL_RATIO" \
    --max-samples "$MAX_SAMPLES" \
    --device "$DEVICE" \
    --num-workers "$NUM_WORKERS" \
    --seed "$SEED" \
    --log-dir "$LOG_ROOT" \
    --run-name "exp_${name}" \
    --epochs "$EPOCHS" \
    "$@"
}

# Generacja pojedynczego eksperymentu.
# Argumenty:
#   $1 nazwa eksperymentu (folder w samples/ + sufiks pliku)
#   $2 checkpoint do użycia
#   pozostałe argumenty – flagi przekazywane do generate.py.
run_generate_experiment() {
  local name="$1"; shift
  local ckpt="$1"; shift
  local out_dir="$SAMPLES_ROOT/exp_${name}"
  local out_json="$out_dir/sample_${LAYOUT_ID}_${GRADE_ID}.json"
  local out_log="$out_dir/console.log"
  mkdir -p "$out_dir"
  echo
  echo "------------------------------------------------------------"
  echo "[B] GENERATE  exp=$name  ckpt=$ckpt"
  echo "    extra args: $*"
  echo "------------------------------------------------------------"
  "$VENV_PYTHON" kilter_dl/generate.py \
    --checkpoint "$ckpt" \
    --token-map "$TOKEN_MAP" \
    --id-map "$ID_MAP" \
    --layout-id "$LAYOUT_ID" \
    --grade-id "$GRADE_ID" \
    --output "$out_json" \
    "$@" | tee "$out_log"
}

# =====================================================================
# A) Eksperymenty treningowe – wpływ parametrów uczenia.
# =====================================================================
if [[ "$ONLY_GENERATE" -ne 1 ]]; then

  # A0 – baseline (zbliżony do run_train.sh).
  run_train_experiment "A0_baseline" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.15

  # A1 – wpływ learning rate (bardzo niski / wysoki).
  run_train_experiment "A1_lr_low" \
    --batch-size 64 --lr 0.00001 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.15

  run_train_experiment "A1_lr_high" \
    --batch-size 64 --lr 0.01 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.15

  # A2 – wpływ rozmiaru modelu (capacity: warstwy + hidden_dim).
  run_train_experiment "A2_small_model" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 64 --hidden-dim 64 --layers 1 --dropout 0.15

  run_train_experiment "A2_large_model" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 256 --hidden-dim 512 --layers 4 --dropout 0.15

  # A3 – wpływ regularyzacji (dropout 0.0 vs 0.5).
  run_train_experiment "A3_dropout_off" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.0

  run_train_experiment "A3_dropout_high" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.5

  # A4 – wpływ batch size (mały szum vs duży batch).
  run_train_experiment "A4_batch_small" \
    --batch-size 16 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.15

  run_train_experiment "A4_batch_large" \
    --batch-size 256 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 3 --dropout 0.15

  # A5 – wpływ liczby warstw GRU (1 vs 5).
  run_train_experiment "A5_layers_1" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 1 --dropout 0.15

  run_train_experiment "A5_layers_5" \
    --batch-size 64 --lr 0.001 \
    --embed-dim 128 --hidden-dim 256 --layers 5 --dropout 0.15

else
  echo "(--only-generate) Pomijam grupę A (trening)."
fi

# =====================================================================
# B) Eksperymenty generacji – wpływ parametrów dekodowania.
#    Używamy jednego, dobrze wytrenowanego modelu jako referencji
#    (domyślnie checkpoint baseline z grupy A albo
#    kilter_dl/checkpoints/kilter_gen.pt jeśli istnieje).
# =====================================================================
if [[ "$ONLY_TRAIN" -ne 1 ]]; then

  GEN_CKPT="${GEN_CHECKPOINT:-}"
  if [[ -z "$GEN_CKPT" ]]; then
    if [[ -f "$CKPT_ROOT/exp_A0_baseline.pt" ]]; then
      GEN_CKPT="$CKPT_ROOT/exp_A0_baseline.pt"
    elif [[ -f "$CKPT_ROOT/kilter_gen.pt" ]]; then
      GEN_CKPT="$CKPT_ROOT/kilter_gen.pt"
    else
      echo "Nie znaleziono żadnego checkpointa do generacji."
      echo "Uruchom najpierw bez --only-generate albo ustaw GEN_CHECKPOINT=..."
      exit 1
    fi
  fi
  echo
  echo "Generacja używa checkpointa: $GEN_CKPT"

  # B0 – greedy (deterministyczny baseline).
  run_generate_experiment "B0_greedy" "$GEN_CKPT" \
    --greedy --max-len 128

  # B1 – wpływ temperatury (sampling, T = 0.5 / 1.0 / 1.5).
  run_generate_experiment "B1_temp_0.5" "$GEN_CKPT" \
    --temperature 0.5 --top-k 0 --top-p 1.0 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B1_temp_1.0" "$GEN_CKPT" \
    --temperature 1.0 --top-k 0 --top-p 1.0 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B1_temp_1.5" "$GEN_CKPT" \
    --temperature 1.5 --top-k 0 --top-p 1.0 --repetition-penalty 1.0 --max-len 128

  # B2 – wpływ top-k (1 / 5 / 40).
  run_generate_experiment "B2_topk_1" "$GEN_CKPT" \
    --temperature 1.0 --top-k 1 --top-p 1.0 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B2_topk_5" "$GEN_CKPT" \
    --temperature 1.0 --top-k 5 --top-p 1.0 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B2_topk_40" "$GEN_CKPT" \
    --temperature 1.0 --top-k 40 --top-p 1.0 --repetition-penalty 1.0 --max-len 128

  # B3 – wpływ top-p (0.5 / 0.9 / 0.99).
  run_generate_experiment "B3_topp_0.5" "$GEN_CKPT" \
    --temperature 1.0 --top-k 0 --top-p 0.5 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B3_topp_0.9" "$GEN_CKPT" \
    --temperature 1.0 --top-k 0 --top-p 0.9 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B3_topp_0.99" "$GEN_CKPT" \
    --temperature 1.0 --top-k 0 --top-p 0.99 --repetition-penalty 1.0 --max-len 128

  # B4 – wpływ repetition penalty (1.0 vs 1.3).
  run_generate_experiment "B4_reppen_1.0" "$GEN_CKPT" \
    --temperature 1.0 --top-k 40 --top-p 0.95 --repetition-penalty 1.0 --max-len 128

  run_generate_experiment "B4_reppen_1.3" "$GEN_CKPT" \
    --temperature 1.0 --top-k 40 --top-p 0.95 --repetition-penalty 1.3 --max-len 128

  # B5 – konfiguracja „kreatywna” vs „zachowawcza” (zestawy parametrów).
  run_generate_experiment "B5_conservative" "$GEN_CKPT" \
    --temperature 0.7 --top-k 10 --top-p 0.9 --repetition-penalty 1.1 --max-len 128

  run_generate_experiment "B5_creative" "$GEN_CKPT" \
    --temperature 1.4 --top-k 60 --top-p 0.98 --repetition-penalty 1.05 --max-len 128

else
  echo "(--only-train) Pomijam grupę B (generacja)."
fi

echo
echo "============================================================"
echo "Eksperymenty zakończone."
echo " Logi treningu : $LOG_ROOT/exp_*/"
echo " Checkpointy   : $CKPT_ROOT/exp_*.pt"
echo " Próbki        : $SAMPLES_ROOT/exp_*/"
echo "============================================================"
