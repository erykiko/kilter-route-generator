from __future__ import annotations

import argparse
import csv
import functools
import json
import random
import time
from datetime import UTC, datetime
from pathlib import Path
from typing import Dict, List, Tuple

import torch
import torch.nn.functional as F
from torch import nn
from torch.utils.data import DataLoader, Dataset

from model import KilterRouteGenerator


class KilterDataset(Dataset):
    def __init__(self, samples: List[dict]) -> None:
        self.samples = samples

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, idx: int) -> Tuple[torch.Tensor, torch.Tensor]:
        item = self.samples[idx]
        source = torch.tensor(item["source"], dtype=torch.long)
        target = torch.tensor(item["target"], dtype=torch.long)
        return source, target


def collate_batch(batch, pad_id: int, sos_id: int, eos_id: int):
    sources, targets = zip(*batch)
    src = torch.stack(sources, dim=0)

    tgt_in_list = []
    tgt_out_list = []
    max_len = 0
    for t in targets:
        tgt_in = torch.cat([torch.tensor([sos_id]), t], dim=0)
        tgt_out = torch.cat([t, torch.tensor([eos_id])], dim=0)
        tgt_in_list.append(tgt_in)
        tgt_out_list.append(tgt_out)
        max_len = max(max_len, tgt_in.shape[0])

    batch_size = len(targets)
    tgt_in_padded = torch.full((batch_size, max_len), pad_id, dtype=torch.long)
    tgt_out_padded = torch.full((batch_size, max_len), pad_id, dtype=torch.long)
    for i, (ti, to) in enumerate(zip(tgt_in_list, tgt_out_list)):
        tgt_in_padded[i, : ti.shape[0]] = ti
        tgt_out_padded[i, : to.shape[0]] = to

    return src, tgt_in_padded, tgt_out_padded


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train Kilter route generator.")
    parser.add_argument("--dataset", type=Path, default=Path("dataset/dataset/single_board.json"))
    parser.add_argument("--token-map", type=Path, default=Path("dataset/token_to_id.json"))
    parser.add_argument("--output", type=Path, default=Path("kilter_dl/checkpoints/kilter_gen.pt"))
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--embed-dim", type=int, default=128)
    parser.add_argument("--hidden-dim", type=int, default=256)
    parser.add_argument("--layers", type=int, default=2)
    parser.add_argument("--dropout", type=float, default=0.2)
    parser.add_argument(
        "--condition-every-step",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Feed layout/grade context into every decoder step, not only h0.",
    )
    parser.add_argument("--val-ratio", type=float, default=0.1)
    parser.add_argument("--max-samples", type=int, default=0)
    parser.add_argument(
        "--device",
        type=str,
        default="cuda",
        choices=["cuda", "cpu"],
        help="Training device. Defaults to CUDA to force GPU usage.",
    )
    parser.add_argument(
        "--num-workers",
        type=int,
        default=2,
        help="DataLoader worker processes.",
    )
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument(
        "--log-dir",
        type=Path,
        default=Path("kilter_dl/logs"),
        help="Directory for training logs (CSV + JSON).",
    )
    parser.add_argument(
        "--run-name",
        type=str,
        default="",
        help="Optional run name. If empty, timestamp is used.",
    )
    return parser.parse_args()


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def build_layout_grade_ids(samples: List[dict]) -> Tuple[Dict[int, int], Dict[int, int]]:
    layouts = sorted({s["source"][0] for s in samples})
    grades = sorted({s["source"][1] for s in samples})
    layout_to_idx = {token_id: i for i, token_id in enumerate(layouts)}
    grade_to_idx = {token_id: i for i, token_id in enumerate(grades)}
    return layout_to_idx, grade_to_idx


def remap_source(sample: dict, layout_to_idx: Dict[int, int], grade_to_idx: Dict[int, int]) -> dict:
    layout_id, grade_id = sample["source"]
    return {
        "source": [layout_to_idx[layout_id], grade_to_idx[grade_id]],
        "target": sample["target"],
    }


def evaluate(model: nn.Module, loader: DataLoader, pad_id: int, device: torch.device) -> float:
    model.eval()
    total_loss = 0.0
    total_tokens = 0
    with torch.no_grad():
        for src, tgt_in, tgt_out in loader:
            non_blocking = device.type == "cuda"
            src = src.to(device, non_blocking=non_blocking)
            tgt_in = tgt_in.to(device, non_blocking=non_blocking)
            tgt_out = tgt_out.to(device, non_blocking=non_blocking)
            logits = model(src, tgt_in)
            loss = F.cross_entropy(
                logits.reshape(-1, logits.shape[-1]),
                tgt_out.reshape(-1),
                ignore_index=pad_id,
                reduction="sum",
            )
            total_loss += loss.item()
            total_tokens += (tgt_out != pad_id).sum().item()
    return total_loss / max(1, total_tokens)


def main() -> None:
    args = parse_args()
    random.seed(args.seed)
    torch.manual_seed(args.seed)
    run_started_at = datetime.now(UTC).isoformat(timespec="seconds")
    run_name = args.run_name or datetime.now(UTC).strftime("%Y%m%d_%H%M%S")

    token_to_id = load_json(args.token_map)
    pad_id = token_to_id["<PAD>"]
    sos_id = token_to_id["<SOS>"]
    eos_id = token_to_id["<EOS>"]
    vocab_size = max(token_to_id.values()) + 1

    samples_raw = load_json(args.dataset)
    if args.max_samples > 0:
        samples_raw = samples_raw[: args.max_samples]
    layout_to_idx, grade_to_idx = build_layout_grade_ids(samples_raw)
    samples = [remap_source(s, layout_to_idx, grade_to_idx) for s in samples_raw]

    random.shuffle(samples)
    split = int(len(samples) * (1.0 - args.val_ratio))
    train_samples = samples[:split]
    val_samples = samples[split:]

    train_ds = KilterDataset(train_samples)
    val_ds = KilterDataset(val_samples)

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError(
            "CUDA device requested but not available. "
            "Check GPU drivers/CUDA, or run with --device cpu."
        )
    device = torch.device(args.device)
    use_cuda = device.type == "cuda"
    if use_cuda:
        torch.backends.cudnn.benchmark = True
        gpu_name = torch.cuda.get_device_name(0)
        print(f"Using CUDA GPU: {gpu_name}")
    else:
        print("Using CPU (no CUDA).")

    collate = functools.partial(collate_batch, pad_id=pad_id, sos_id=sos_id, eos_id=eos_id)
    train_loader = DataLoader(
        train_ds,
        batch_size=args.batch_size,
        shuffle=True,
        collate_fn=collate,
        num_workers=args.num_workers,
        pin_memory=use_cuda,
    )
    val_loader = DataLoader(
        val_ds,
        batch_size=args.batch_size,
        shuffle=False,
        collate_fn=collate,
        num_workers=args.num_workers,
        pin_memory=use_cuda,
    )

    model = KilterRouteGenerator(
        vocab_size=vocab_size,
        layout_vocab_size=len(layout_to_idx),
        grade_vocab_size=len(grade_to_idx),
        embed_dim=args.embed_dim,
        hidden_dim=args.hidden_dim,
        num_layers=args.layers,
        dropout=args.dropout,
        condition_every_step=args.condition_every_step,
    ).to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr)

    best_val = float("inf")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.log_dir.mkdir(parents=True, exist_ok=True)
    run_dir = args.log_dir / run_name
    run_dir.mkdir(parents=True, exist_ok=True)
    metrics_csv_path = run_dir / "metrics.csv"
    metrics_jsonl_path = run_dir / "metrics.jsonl"
    summary_json_path = run_dir / "run_summary.json"

    with metrics_csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["epoch", "train_nll", "val_nll", "val_ppl", "epoch_seconds", "is_best"])

    for epoch in range(1, args.epochs + 1):
        epoch_start = time.perf_counter()
        model.train()
        running_loss = 0.0
        running_tokens = 0

        for src, tgt_in, tgt_out in train_loader:
            src = src.to(device, non_blocking=use_cuda)
            tgt_in = tgt_in.to(device, non_blocking=use_cuda)
            tgt_out = tgt_out.to(device, non_blocking=use_cuda)

            optimizer.zero_grad(set_to_none=True)
            logits = model(src, tgt_in)
            loss = F.cross_entropy(
                logits.reshape(-1, logits.shape[-1]),
                tgt_out.reshape(-1),
                ignore_index=pad_id,
                reduction="sum",
            )
            token_count = (tgt_out != pad_id).sum().item()
            (loss / max(1, token_count)).backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimizer.step()

            running_loss += loss.item()
            running_tokens += token_count

        train_nll = running_loss / max(1, running_tokens)
        val_nll = evaluate(model, val_loader, pad_id=pad_id, device=device)
        val_ppl = torch.exp(torch.tensor(val_nll)).item()
        epoch_seconds = time.perf_counter() - epoch_start

        print(
            f"Epoch {epoch:02d} | train_nll={train_nll:.4f} | "
            f"val_nll={val_nll:.4f} | ppl={val_ppl:.2f}"
        )

        is_best = False
        if val_nll < best_val:
            best_val = val_nll
            is_best = True
            ckpt = {
                "model_state": model.state_dict(),
                "config": {
                    "vocab_size": vocab_size,
                    "layout_vocab_size": len(layout_to_idx),
                    "grade_vocab_size": len(grade_to_idx),
                    "embed_dim": args.embed_dim,
                    "hidden_dim": args.hidden_dim,
                    "num_layers": args.layers,
                    "dropout": args.dropout,
                    "condition_every_step": args.condition_every_step,
                },
                "pad_id": pad_id,
                "sos_id": sos_id,
                "eos_id": eos_id,
                "layout_to_idx": layout_to_idx,
                "grade_to_idx": grade_to_idx,
            }
            torch.save(ckpt, args.output)
            print(f"  Saved best checkpoint to {args.output}")

        with metrics_csv_path.open("a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(
                [epoch, f"{train_nll:.6f}", f"{val_nll:.6f}", f"{val_ppl:.6f}", f"{epoch_seconds:.4f}", int(is_best)]
            )

        with metrics_jsonl_path.open("a", encoding="utf-8") as f:
            f.write(
                json.dumps(
                    {
                        "epoch": epoch,
                        "train_nll": train_nll,
                        "val_nll": val_nll,
                        "val_ppl": val_ppl,
                        "epoch_seconds": epoch_seconds,
                        "is_best": is_best,
                    }
                )
                + "\n"
            )

    run_summary = {
        "run_name": run_name,
        "started_at_utc": run_started_at,
        "dataset": str(args.dataset),
        "token_map": str(args.token_map),
        "checkpoint_output": str(args.output),
        "device": str(device),
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "lr": args.lr,
        "embed_dim": args.embed_dim,
        "hidden_dim": args.hidden_dim,
        "layers": args.layers,
        "dropout": args.dropout,
        "condition_every_step": args.condition_every_step,
        "val_ratio": args.val_ratio,
        "max_samples": args.max_samples,
        "seed": args.seed,
        "num_workers": args.num_workers,
        "num_samples_total": len(samples),
        "num_samples_train": len(train_samples),
        "num_samples_val": len(val_samples),
        "best_val_nll": best_val,
        "metrics_csv": str(metrics_csv_path),
        "metrics_jsonl": str(metrics_jsonl_path),
    }
    with summary_json_path.open("w", encoding="utf-8") as f:
        json.dump(run_summary, f, indent=2)

    print(f"Logs saved to: {run_dir}")
    print(f"Training finished. Best val_nll={best_val:.4f}")


if __name__ == "__main__":
    main()
