"""Funkcje rysujące wykresy do raportu (zapisywane jako PDF wektorowe)."""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Sequence

import matplotlib.pyplot as plt
import numpy as np

from .data import GenRun, TrainRun

# Globalne ustawienia stylu zgodne z formatem raportu LaTeX (czcionka serif).
plt.rcParams.update(
    {
        "font.family": "serif",
        "font.size": 10,
        "axes.titlesize": 11,
        "axes.labelsize": 10,
        "legend.fontsize": 9,
        "xtick.labelsize": 9,
        "ytick.labelsize": 9,
        "figure.dpi": 110,
        "savefig.dpi": 200,
        "axes.grid": True,
        "grid.alpha": 0.25,
    }
)


def _save(fig, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(out_path, format="pdf", bbox_inches="tight")
    plt.close(fig)


def _label_for(run: TrainRun, key: str | None = None) -> str:
    s = run.summary
    if key == "lr":
        return f"lr={s.get('lr')}"
    if key == "capacity":
        return f"e={s.get('embed_dim')}, h={s.get('hidden_dim')}, L={s.get('layers')}"
    if key == "dropout":
        return f"dropout={s.get('dropout')}"
    if key == "batch":
        return f"bs={s.get('batch_size')}"
    if key == "layers":
        return f"L={s.get('layers')}"
    return run.name


def plot_training_curves(
    runs: Sequence[TrainRun],
    out_path: Path,
    title: str,
    label_key: str | None = None,
) -> None:
    """train_nll/val_nll w funkcji epoki dla zestawu przebiegów."""
    fig, ax = plt.subplots(figsize=(6.0, 3.6))
    cmap = plt.get_cmap("tab10")
    for i, run in enumerate(runs):
        color = cmap(i % 10)
        label = _label_for(run, label_key)
        ax.plot(run.epochs, run.train_nll, color=color, linestyle="--", marker="o",
                markersize=3, linewidth=1.0, alpha=0.7,
                label=f"{label} (train)")
        ax.plot(run.epochs, run.val_nll, color=color, linestyle="-", marker="s",
                markersize=3, linewidth=1.4,
                label=f"{label} (val)")
    ax.set_xlabel("Epoka")
    ax.set_ylabel("NLL (na token)")
    ax.set_title(title)
    ax.legend(loc="best", ncols=1, frameon=True)
    _save(fig, out_path)


def plot_best_val_summary(runs: Dict[str, TrainRun], out_path: Path) -> None:
    items = sorted(runs.items(), key=lambda kv: kv[1].best_val)
    names = [k.replace("exp_", "") for k, _ in items]
    vals = [v.best_val for _, v in items]

    fig, ax = plt.subplots(figsize=(7.5, 3.8))
    colors = plt.get_cmap("viridis")(np.linspace(0.1, 0.9, len(names)))
    bars = ax.barh(names, vals, color=colors, edgecolor="black", linewidth=0.4)
    ax.set_xlabel("Najlepsze val\\_nll (mniej = lepiej)")
    ax.set_title("Porównanie najlepszego val\\_nll – grupa A (uczenie)")
    ax.invert_yaxis()
    for bar, v in zip(bars, vals):
        ax.text(v + 0.02, bar.get_y() + bar.get_height() / 2, f"{v:.3f}",
                va="center", fontsize=8)
    _save(fig, out_path)


def plot_epoch_time_summary(runs: Dict[str, TrainRun], out_path: Path) -> None:
    items = sorted(runs.items(), key=lambda kv: kv[1].mean_epoch_seconds)
    names = [k.replace("exp_", "") for k, _ in items]
    vals = [v.mean_epoch_seconds for _, v in items]

    fig, ax = plt.subplots(figsize=(7.5, 3.8))
    colors = plt.get_cmap("plasma")(np.linspace(0.15, 0.85, len(names)))
    bars = ax.barh(names, vals, color=colors, edgecolor="black", linewidth=0.4)
    ax.set_xlabel("Średni czas epoki [s]")
    ax.set_title("Koszt obliczeniowy epoki – grupa A")
    ax.invert_yaxis()
    for bar, v in zip(bars, vals):
        ax.text(v + max(vals) * 0.01, bar.get_y() + bar.get_height() / 2,
                f"{v:.1f} s", va="center", fontsize=8)
    _save(fig, out_path)


def plot_gen_lengths(runs: Dict[str, GenRun], out_path: Path) -> None:
    items = list(runs.items())
    names = [k.replace("exp_", "") for k, _ in items]
    holes = [v.num_holes for _, v in items]
    unique = [v.num_unique_holes for _, v in items]

    x = np.arange(len(names))
    fig, ax = plt.subplots(figsize=(8.0, 3.6))
    width = 0.4
    ax.bar(x - width / 2, holes, width, label="Liczba chwytów (z powtórzeniami)",
           color="#3b82f6", edgecolor="black", linewidth=0.4)
    ax.bar(x + width / 2, unique, width, label="Liczba unikalnych chwytów",
           color="#f59e0b", edgecolor="black", linewidth=0.4)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=45, ha="right")
    ax.set_ylabel("Liczba")
    ax.set_title("Długość trasy a różnorodność chwytów – grupa B")
    ax.legend(loc="best")
    _save(fig, out_path)


def plot_gen_unique_ratio(runs: Dict[str, GenRun], out_path: Path) -> None:
    items = list(runs.items())
    names = [k.replace("exp_", "") for k, _ in items]
    ratio = [v.unique_ratio * 100 for _, v in items]

    fig, ax = plt.subplots(figsize=(8.0, 3.4))
    colors = plt.get_cmap("coolwarm")(np.linspace(0.15, 0.85, len(names)))
    bars = ax.bar(names, ratio, color=colors, edgecolor="black", linewidth=0.4)
    ax.set_ylabel("Unikalne / wszystkie chwyty [%]")
    ax.set_title("Udział unikalnych chwytów w generowanej trasie – grupa B")
    ax.set_ylim(0, 110)
    for bar, v in zip(bars, ratio):
        ax.text(bar.get_x() + bar.get_width() / 2, v + 1.5, f"{v:.0f}%",
                ha="center", fontsize=8)
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right")
    _save(fig, out_path)


def _line_param_plot(
    runs: Sequence[GenRun],
    xs: Sequence[float],
    out_path: Path,
    xlabel: str,
    title: str,
    log_x: bool = False,
) -> None:
    holes = [r.num_holes for r in runs]
    unique = [r.num_unique_holes for r in runs]

    fig, ax1 = plt.subplots(figsize=(6.0, 3.6))
    ax1.plot(xs, holes, "o-", color="#1d4ed8", linewidth=1.4,
             label="Liczba chwytów")
    ax1.plot(xs, unique, "s--", color="#f59e0b", linewidth=1.4,
             label="Liczba unikalnych chwytów")
    ax1.set_xlabel(xlabel)
    ax1.set_ylabel("Liczba chwytów")
    if log_x:
        ax1.set_xscale("log")
    ax1.legend(loc="best")
    ax1.set_title(title)
    _save(fig, out_path)


def plot_temperature_sweep(b1_runs: Dict[str, GenRun], out_path: Path) -> None:
    order = ["exp_B1_temp_0.5", "exp_B1_temp_1.0", "exp_B1_temp_1.5"]
    runs = [b1_runs[k] for k in order if k in b1_runs]
    xs = [0.5, 1.0, 1.5][: len(runs)]
    _line_param_plot(runs, xs, out_path,
                     xlabel=r"Temperatura $\tau$",
                     title="Wpływ temperatury na trasę")


def plot_topk_sweep(b2_runs: Dict[str, GenRun], out_path: Path) -> None:
    order = ["exp_B2_topk_1", "exp_B2_topk_5", "exp_B2_topk_40"]
    runs = [b2_runs[k] for k in order if k in b2_runs]
    xs = [1, 5, 40][: len(runs)]
    _line_param_plot(runs, xs, out_path,
                     xlabel=r"Top-$k$",
                     title=r"Wpływ top-$k$ na trasę",
                     log_x=True)


def plot_topp_sweep(b3_runs: Dict[str, GenRun], out_path: Path) -> None:
    order = ["exp_B3_topp_0.5", "exp_B3_topp_0.9", "exp_B3_topp_0.99"]
    runs = [b3_runs[k] for k in order if k in b3_runs]
    xs = [0.5, 0.9, 0.99][: len(runs)]
    _line_param_plot(runs, xs, out_path,
                     xlabel=r"Top-$p$",
                     title=r"Wpływ top-$p$ na trasę")


def plot_reppen_compare(b4_runs: Dict[str, GenRun], out_path: Path) -> None:
    order = ["exp_B4_reppen_1.0", "exp_B4_reppen_1.3"]
    runs = [b4_runs[k] for k in order if k in b4_runs]
    xs = [1.0, 1.3][: len(runs)]
    _line_param_plot(runs, xs, out_path,
                     xlabel=r"Repetition penalty $\rho$",
                     title=r"Wpływ kary za powtórzenia $\rho$")


__all__ = [
    "plot_training_curves",
    "plot_best_val_summary",
    "plot_epoch_time_summary",
    "plot_gen_lengths",
    "plot_gen_unique_ratio",
    "plot_temperature_sweep",
    "plot_topk_sweep",
    "plot_topp_sweep",
    "plot_reppen_compare",
]
