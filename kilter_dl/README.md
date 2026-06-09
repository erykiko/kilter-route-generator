# Kilter DL (PyTorch)

This folder contains a deep learning pipeline that learns to generate a Kilter route token sequence from:
- board layout token
- grade token

It uses:
- input: `source = [layout_id, grade_id]`
- target: `target = [route tokens...]`

## 1) Create and activate virtual environment

From project root:

```bash
python -m venv kilter_dl/.venv
source kilter_dl/.venv/bin/activate
pip install -r kilter_dl/requirements.txt
```

## 2) Train model

```bash
python kilter_dl/train.py \
  --dataset dataset/dataset/single_board.json \
  --token-map dataset/token_to_id.json \
  --output kilter_dl/checkpoints/kilter_gen.pt \
  --device cuda \
  --epochs 10 \
  --batch-size 128
```

Notes:
- Uses GPU automatically if available.
- Saves the best checkpoint by validation NLL.
- With default settings, training requires CUDA and fails fast if GPU is not available.
- If you need CPU fallback, pass `--device cpu`.
- Saves logs for each run in `kilter_dl/logs/<run_name>/`:
  - `metrics.csv` (easy for spreadsheets/reporting),
  - `metrics.jsonl` (machine-friendly),
  - `run_summary.json` (config + dataset metadata + best metric).

You can set a fixed run name for cleaner reports:

```bash
python kilter_dl/train.py --run-name exp_subset_1270
```

## 3) Generate route from layout + grade

By token IDs:

```bash
python kilter_dl/generate.py \
  --checkpoint kilter_dl/checkpoints/kilter_gen.pt \
  --layout-id 1270 \
  --grade-id 1239 \
  --output kilter_dl/samples/sample_1270_1239.json
```

By token names:

```bash
python kilter_dl/generate.py \
  --checkpoint kilter_dl/checkpoints/kilter_gen.pt \
  --layout-name "12 x 14 Commerical" \
  --grade-name "6a/V3"
```
