"""CLI entry point: generuje wszystkie wykresy do report/figures/."""

from __future__ import annotations

import argparse
from pathlib import Path

from .data import load_gen_runs, load_train_runs
from . import plots


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Generuj wykresy do raportu.")
    p.add_argument("--logs-dir", type=Path, default=Path("kilter_dl/logs"))
    p.add_argument("--samples-dir", type=Path, default=Path("kilter_dl/samples"))
    p.add_argument("--output-dir", type=Path, default=Path("report/figures"))
    return p.parse_args()


def main() -> None:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    train_runs = load_train_runs(args.logs_dir, prefix="exp_A")
    if not train_runs:
        raise SystemExit(f"Brak logów uczenia w {args.logs_dir}/exp_A*")

    gen_runs = load_gen_runs(args.samples_dir, prefix="exp_B")
    if not gen_runs:
        raise SystemExit(f"Brak próbek generacji w {args.samples_dir}/exp_B*")

    print(f"Wczytano {len(train_runs)} run(ów) treningowych "
          f"i {len(gen_runs)} run(ów) generacji.")

    def pick(*keys):
        return [train_runs[k] for k in keys if k in train_runs]

    plots.plot_training_curves(
        pick("exp_A1_lr_low", "exp_A0_baseline", "exp_A1_lr_high"),
        args.output_dir / "fig_train_lr.pdf",
        title="Wpływ learning rate na uczenie",
        label_key="lr",
    )
    plots.plot_training_curves(
        pick("exp_A2_small_model", "exp_A0_baseline", "exp_A2_large_model"),
        args.output_dir / "fig_train_capacity.pdf",
        title="Wpływ pojemności modelu na uczenie",
        label_key="capacity",
    )
    plots.plot_training_curves(
        pick("exp_A3_dropout_off", "exp_A0_baseline", "exp_A3_dropout_high"),
        args.output_dir / "fig_train_dropout.pdf",
        title="Wpływ dropoutu na uczenie",
        label_key="dropout",
    )
    plots.plot_training_curves(
        pick("exp_A4_batch_small", "exp_A0_baseline", "exp_A4_batch_large"),
        args.output_dir / "fig_train_batch.pdf",
        title="Wpływ rozmiaru batcha na uczenie",
        label_key="batch",
    )
    plots.plot_training_curves(
        pick("exp_A5_layers_1", "exp_A0_baseline", "exp_A5_layers_5"),
        args.output_dir / "fig_train_layers.pdf",
        title="Wpływ liczby warstw GRU na uczenie",
        label_key="layers",
    )

    plots.plot_best_val_summary(train_runs, args.output_dir / "fig_summary_best_val.pdf")
    plots.plot_epoch_time_summary(train_runs, args.output_dir / "fig_summary_epoch_time.pdf")

    plots.plot_gen_lengths(gen_runs, args.output_dir / "fig_gen_lengths.pdf")
    plots.plot_gen_unique_ratio(gen_runs, args.output_dir / "fig_gen_unique_ratio.pdf")
    plots.plot_temperature_sweep(gen_runs, args.output_dir / "fig_gen_temperature.pdf")
    plots.plot_topk_sweep(gen_runs, args.output_dir / "fig_gen_topk.pdf")
    plots.plot_topp_sweep(gen_runs, args.output_dir / "fig_gen_topp.pdf")
    plots.plot_reppen_compare(gen_runs, args.output_dir / "fig_gen_reppen.pdf")

    print(f"Zapisano wykresy do: {args.output_dir}")


if __name__ == "__main__":
    main()
