#!/usr/bin/env python3
"""Create a dataset subset that contains only one board layout."""

import argparse
import json
from collections import Counter
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Filter climbing samples to a single board layout."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("dataset/dataset/single_board.json"),
        help="Path to input dataset JSON.",
    )
    parser.add_argument(
        "--token-map",
        type=Path,
        default=Path("dataset/id_to_token.json"),
        help="Path to id_to_token.json mapping file.",
    )
    parser.add_argument(
        "--layout-id",
        type=int,
        help="Board layout token ID (e.g. 1270).",
    )
    parser.add_argument(
        "--layout-name",
        type=str,
        help='Board layout name (e.g. "12 x 14 Commerical").',
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output path for filtered JSON. Defaults to dataset/dataset/single_layout_<ID>.json.",
    )
    return parser.parse_args()


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def main() -> None:
    args = parse_args()
    data = load_json(args.input)
    id_to_token_raw = load_json(args.token_map)
    id_to_token = {int(k): v for k, v in id_to_token_raw.items()}
    token_to_id = {v: k for k, v in id_to_token.items()}

    layout_counts = Counter(item["source"][0] for item in data if item.get("source"))
    print(f"Total samples: {len(data)}")
    print(f"Detected layouts: {len(layout_counts)}")
    print("Layout counts:")
    for layout_id, count in layout_counts.most_common():
        name = id_to_token.get(layout_id, "<unknown>")
        print(f"  - {layout_id:>4} | {name:<35} | {count}")

    selected_layout_id = args.layout_id
    if selected_layout_id is None and args.layout_name:
        if args.layout_name not in token_to_id:
            raise ValueError(
                f'Unknown layout name "{args.layout_name}". '
                "Use one of the names printed above."
            )
        selected_layout_id = token_to_id[args.layout_name]

    if selected_layout_id is None:
        raise ValueError("Provide either --layout-id or --layout-name.")

    if selected_layout_id not in layout_counts:
        raise ValueError(f"Layout ID {selected_layout_id} not found in dataset.")

    subset = [item for item in data if item.get("source") and item["source"][0] == selected_layout_id]
    output_path = args.output or Path(f"dataset/dataset/single_layout_{selected_layout_id}.json")
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as f:
        json.dump(subset, f, ensure_ascii=False)

    layout_name = id_to_token.get(selected_layout_id, "<unknown>")
    print("")
    print(f"Selected layout: {selected_layout_id} ({layout_name})")
    print(f"Subset size: {len(subset)}")
    print(f"Saved to: {output_path}")


if __name__ == "__main__":
    main()
