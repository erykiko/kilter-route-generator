from __future__ import annotations

import argparse
import json
from pathlib import Path

import torch

from model import KilterRouteGenerator


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Kilter route tokens.")
    parser.add_argument("--checkpoint", type=Path, default=Path("kilter_dl/checkpoints/kilter_gen.pt"))
    parser.add_argument("--token-map", type=Path, default=Path("dataset/token_to_id.json"))
    parser.add_argument("--id-map", type=Path, default=Path("dataset/id_to_token.json"))
    parser.add_argument("--layout-id", type=int)
    parser.add_argument("--layout-name", type=str)
    parser.add_argument("--grade-id", type=int)
    parser.add_argument("--grade-name", type=str)
    parser.add_argument("--max-len", type=int, default=128)
    parser.add_argument("--greedy", action="store_true", help="Use greedy decoding (deterministic).")
    parser.add_argument("--temperature", type=float, default=1.15)
    parser.add_argument("--top-k", type=int, default=40)
    parser.add_argument("--top-p", type=float, default=0.95)
    parser.add_argument("--repetition-penalty", type=float, default=1.1)
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def choose_token_id(
    explicit_id: int | None,
    explicit_name: str | None,
    token_to_id: dict,
    kind: str,
) -> int:
    if explicit_id is not None:
        return explicit_id
    if explicit_name is not None:
        if explicit_name not in token_to_id:
            raise ValueError(f'Unknown {kind} name "{explicit_name}".')
        return token_to_id[explicit_name]
    raise ValueError(f"Provide --{kind}-id or --{kind}-name")


def main() -> None:
    args = parse_args()
    ckpt = torch.load(args.checkpoint, map_location="cpu")
    config = ckpt["config"]
    token_to_id = load_json(args.token_map)
    id_to_token_raw = load_json(args.id_map)
    id_to_token = {int(k): v for k, v in id_to_token_raw.items()}

    layout_id = choose_token_id(args.layout_id, args.layout_name, token_to_id, "layout")
    grade_id = choose_token_id(args.grade_id, args.grade_name, token_to_id, "grade")

    layout_to_idx = {int(k): v for k, v in ckpt["layout_to_idx"].items()}
    grade_to_idx = {int(k): v for k, v in ckpt["grade_to_idx"].items()}

    if layout_id not in layout_to_idx:
        raise ValueError(f"Layout token id {layout_id} not seen in training data.")
    if grade_id not in grade_to_idx:
        raise ValueError(f"Grade token id {grade_id} not seen in training data.")

    model = KilterRouteGenerator(**config)
    model.load_state_dict(ckpt["model_state"])
    model.eval()

    source = torch.tensor(
        [[layout_to_idx[layout_id], grade_to_idx[grade_id]]],
        dtype=torch.long,
    )
    generated = model.generate(
        source=source,
        sos_id=ckpt["sos_id"],
        eos_id=ckpt["eos_id"],
        max_len=args.max_len,
        do_sample=not args.greedy,
        temperature=args.temperature,
        top_k=args.top_k,
        top_p=args.top_p,
        repetition_penalty=args.repetition_penalty,
    )[0].tolist()

    route_ids = []
    for token_id in generated:
        if token_id == ckpt["eos_id"]:
            break
        if token_id in (ckpt["sos_id"], ckpt["pad_id"]):
            continue
        route_ids.append(token_id)

    route_tokens = [id_to_token.get(tid, f"<UNK:{tid}>") for tid in route_ids]
    route_hole_ids = []
    for tok in route_tokens:
        if tok.startswith("p") and tok[1:].isdigit():
            route_hole_ids.append(int(tok[1:]))
    source_names = {
        "layout": id_to_token.get(layout_id, str(layout_id)),
        "grade": id_to_token.get(grade_id, str(grade_id)),
    }

    print("Prompt:")
    print(f"  layout={layout_id} ({source_names['layout']})")
    print(f"  grade={grade_id} ({source_names['grade']})")
    mode = "greedy" if args.greedy else "sampling"
    print(f"  decode={mode}, temperature={args.temperature}, top_k={args.top_k}, top_p={args.top_p}, repetition_penalty={args.repetition_penalty}")
    print("Generated route token ids:")
    print(route_ids)
    print("Generated route tokens:")
    print(route_tokens)
    print("Generated hole ids:")
    print(route_hole_ids)

    if args.output:
        payload = {
            "source": [layout_id, grade_id],
            "source_names": source_names,
            "target": route_ids,
            "target_tokens": route_tokens,
            "target_hole_ids": route_hole_ids,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        print(f"Saved generation to {args.output}")


if __name__ == "__main__":
    main()
