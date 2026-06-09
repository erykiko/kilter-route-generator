"""Ładowanie logów eksperymentów kilter_dl."""

from __future__ import annotations

import csv
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List


@dataclass
class TrainRun:
    name: str
    summary: dict
    epochs: List[int] = field(default_factory=list)
    train_nll: List[float] = field(default_factory=list)
    val_nll: List[float] = field(default_factory=list)
    val_ppl: List[float] = field(default_factory=list)
    epoch_seconds: List[float] = field(default_factory=list)

    @property
    def best_val(self) -> float:
        return float(self.summary.get("best_val_nll", min(self.val_nll, default=float("nan"))))

    @property
    def mean_epoch_seconds(self) -> float:
        return sum(self.epoch_seconds) / max(1, len(self.epoch_seconds))


@dataclass
class GenRun:
    name: str
    source: List[int]
    source_names: Dict[str, str]
    target: List[int]
    target_tokens: List[str]
    target_hole_ids: List[int]
    decode_params: Dict[str, str] = field(default_factory=dict)

    @property
    def num_holes(self) -> int:
        return len(self.target_hole_ids)

    @property
    def num_unique_holes(self) -> int:
        return len(set(self.target_hole_ids))

    @property
    def unique_ratio(self) -> float:
        if not self.target_hole_ids:
            return 0.0
        return self.num_unique_holes / len(self.target_hole_ids)

    @property
    def num_tokens(self) -> int:
        return len(self.target)


def load_train_run(run_dir: Path) -> TrainRun:
    csv_path = run_dir / "metrics.csv"
    summary_path = run_dir / "run_summary.json"
    summary = {}
    if summary_path.exists():
        with summary_path.open("r", encoding="utf-8") as f:
            summary = json.load(f)
    run = TrainRun(name=run_dir.name, summary=summary)
    if csv_path.exists():
        with csv_path.open("r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                run.epochs.append(int(row["epoch"]))
                run.train_nll.append(float(row["train_nll"]))
                run.val_nll.append(float(row["val_nll"]))
                run.val_ppl.append(float(row["val_ppl"]))
                run.epoch_seconds.append(float(row["epoch_seconds"]))
    return run


def load_train_runs(logs_dir: Path, prefix: str = "exp_A") -> Dict[str, TrainRun]:
    runs: Dict[str, TrainRun] = {}
    for d in sorted(logs_dir.glob(f"{prefix}*")):
        if not d.is_dir():
            continue
        runs[d.name] = load_train_run(d)
    return runs


def _parse_decode_params(console_log: Path) -> Dict[str, str]:
    params: Dict[str, str] = {}
    if not console_log.exists():
        return params
    text = console_log.read_text(encoding="utf-8")
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("decode="):
            for chunk in line.split(","):
                chunk = chunk.strip()
                if "=" in chunk:
                    k, v = chunk.split("=", 1)
                    params[k.strip()] = v.strip()
    return params


def load_gen_run(run_dir: Path) -> GenRun | None:
    sample_files = sorted(run_dir.glob("sample_*.json"))
    if not sample_files:
        return None
    sample = json.loads(sample_files[0].read_text(encoding="utf-8"))
    decode = _parse_decode_params(run_dir / "console.log")
    return GenRun(
        name=run_dir.name,
        source=sample.get("source", []),
        source_names=sample.get("source_names", {}),
        target=sample.get("target", []),
        target_tokens=sample.get("target_tokens", []),
        target_hole_ids=sample.get("target_hole_ids", []),
        decode_params=decode,
    )


def load_gen_runs(samples_dir: Path, prefix: str = "exp_B") -> Dict[str, GenRun]:
    runs: Dict[str, GenRun] = {}
    for d in sorted(samples_dir.glob(f"{prefix}*")):
        if not d.is_dir():
            continue
        run = load_gen_run(d)
        if run is not None:
            runs[d.name] = run
    return runs
