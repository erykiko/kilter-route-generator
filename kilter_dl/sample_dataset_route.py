from __future__ import annotations

import argparse
import json
import random
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Sample random route from dataset.")
    parser.add_argument("--dataset", type=Path, default=Path("dataset/dataset/single_board.json"))
    parser.add_argument("--id-map", type=Path, default=Path("dataset/id_to_token.json"))
    parser.add_argument("--layout-id", type=int, required=True)
    parser.add_argument("--grade-id", type=int, required=True)
    parser.add_argument("--seed", type=int, default=None)
    return parser.parse_args()


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def decode_hole_ids(target_ids: list[int], id_to_token: dict[int, str]) -> list[int]:
    out: list[int] = []
    for tid in target_ids:
        tok = id_to_token.get(tid, "")
        if tok.startswith("p") and tok[1:].isdigit():
            out.append(int(tok[1:]))
    return out


def decode_route_tokens(target_ids: list[int], id_to_token: dict[int, str]) -> list[str]:
    out: list[str] = []
    for tid in target_ids:
        tok = id_to_token.get(tid, "")
        if tok:
            out.append(tok)
    return out


def main() -> None:
    args = parse_args()
    if args.seed is not None:
        random.seed(args.seed)

    data = load_json(args.dataset)
    id_map_raw = load_json(args.id_map)
    id_to_token = {int(k): v for k, v in id_map_raw.items()}

    candidates = [
        sample
        for sample in data
        if sample.get("source")
        and len(sample["source"]) >= 2
        and sample["source"][0] == args.layout_id
        and sample["source"][1] == args.grade_id
    ]
    if not candidates:
        candidates = [sample for sample in data if sample.get("source") and sample["source"][0] == args.layout_id]
    if not candidates:
        candidates = data

    sample = random.choice(candidates)
    target_ids = sample.get("target", [])
    hole_ids = decode_hole_ids(target_ids, id_to_token)
    route_tokens = decode_route_tokens(target_ids, id_to_token)

    print("Dataset sample source:")
    print(sample.get("source", []))
    print("Dataset target token ids:")
    print(target_ids)
    print("Dataset route tokens:")
    print(route_tokens)
    print("Dataset hole ids:")
    print(hole_ids)


if __name__ == "__main__":
    main()
