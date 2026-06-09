"""Persistent Kilter route generation server (inter-process worker).

This script is meant to be launched once by the C++ GUI as a long-lived child
process. It speaks a tiny line-delimited JSON protocol over stdin/stdout:

  * The C++ parent writes ONE request per line to this process' stdin.
  * This process writes ONE response per line to its stdout.
  * All human-readable logging goes to stderr so it never corrupts the channel.

Keeping the process alive means the (slow) ``import torch`` and per-checkpoint
model loading happen only once, instead of on every "Generate" click as the old
one-shot ``generate.py`` invocation did.

Request objects (one compact JSON object per line)::

    {"cmd": "generate", "checkpoint": "...", "layout_id": 1270, "grade_id": 1239,
     "max_len": 128, "greedy": false, "temperature": 1.15, "top_k": 40,
     "top_p": 0.95, "repetition_penalty": 1.1}

    {"cmd": "ping"}
    {"cmd": "shutdown"}

Response objects::

    {"status": "ready"}                                  # emitted once at startup
    {"status": "ok", "tokens": [...], "hole_ids": [...], "token_ids": [...]}
    {"status": "error", "message": "..."}
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def log(message: str) -> None:
    """Write a diagnostic line to stderr (never to the protocol channel)."""
    print(f"[generate_server] {message}", file=sys.stderr, flush=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Persistent Kilter route generation worker.")
    parser.add_argument("--token-map", type=Path, default=Path("dataset/token_to_id.json"))
    parser.add_argument("--id-map", type=Path, default=Path("dataset/id_to_token.json"))
    return parser.parse_args()


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


class ModelCache:
    """Lazily loads and caches one model per checkpoint path."""

    def __init__(self, token_map_path: Path, id_map_path: Path) -> None:
        import torch  # imported lazily so startup errors are reported cleanly

        from model import KilterRouteGenerator

        self._torch = torch
        self._model_cls = KilterRouteGenerator
        self._token_to_id = load_json(token_map_path)
        id_map_raw = load_json(id_map_path)
        self._id_to_token = {int(k): v for k, v in id_map_raw.items()}
        self._cache: dict[str, dict] = {}

    @property
    def id_to_token(self) -> dict[int, str]:
        return self._id_to_token

    def _load_checkpoint(self, checkpoint: str) -> dict:
        cached = self._cache.get(checkpoint)
        if cached is not None:
            return cached

        log(f"loading checkpoint: {checkpoint}")
        torch = self._torch
        ckpt = torch.load(checkpoint, map_location="cpu")
        config = ckpt["config"]
        model = self._model_cls(**config)
        model.load_state_dict(ckpt["model_state"])
        model.eval()

        bundle = {
            "model": model,
            "layout_to_idx": {int(k): v for k, v in ckpt["layout_to_idx"].items()},
            "grade_to_idx": {int(k): v for k, v in ckpt["grade_to_idx"].items()},
            "sos_id": ckpt["sos_id"],
            "eos_id": ckpt["eos_id"],
            "pad_id": ckpt["pad_id"],
        }
        self._cache[checkpoint] = bundle
        log(f"checkpoint ready ({len(self._cache)} cached)")
        return bundle

    def generate(self, request: dict) -> dict:
        checkpoint = request.get("checkpoint")
        if not checkpoint:
            raise ValueError("Missing 'checkpoint' in request.")
        layout_id = request.get("layout_id")
        grade_id = request.get("grade_id")
        if layout_id is None or grade_id is None:
            raise ValueError("Both 'layout_id' and 'grade_id' are required.")

        bundle = self._load_checkpoint(checkpoint)
        layout_to_idx = bundle["layout_to_idx"]
        grade_to_idx = bundle["grade_to_idx"]
        if layout_id not in layout_to_idx:
            raise ValueError(f"Layout token id {layout_id} not seen in training data.")
        if grade_id not in grade_to_idx:
            raise ValueError(f"Grade token id {grade_id} not seen in training data.")

        torch = self._torch
        source = torch.tensor(
            [[layout_to_idx[layout_id], grade_to_idx[grade_id]]],
            dtype=torch.long,
        )
        greedy = bool(request.get("greedy", False))
        generated = bundle["model"].generate(
            source=source,
            sos_id=bundle["sos_id"],
            eos_id=bundle["eos_id"],
            max_len=int(request.get("max_len", 128)),
            do_sample=not greedy,
            temperature=float(request.get("temperature", 1.15)),
            top_k=int(request.get("top_k", 40)),
            top_p=float(request.get("top_p", 0.95)),
            repetition_penalty=float(request.get("repetition_penalty", 1.1)),
        )[0].tolist()

        route_ids: list[int] = []
        for token_id in generated:
            if token_id == bundle["eos_id"]:
                break
            if token_id in (bundle["sos_id"], bundle["pad_id"]):
                continue
            route_ids.append(token_id)

        route_tokens = [self._id_to_token.get(tid, f"<UNK:{tid}>") for tid in route_ids]
        route_hole_ids = [int(t[1:]) for t in route_tokens if t.startswith("p") and t[1:].isdigit()]
        return {
            "status": "ok",
            "token_ids": route_ids,
            "tokens": route_tokens,
            "hole_ids": route_hole_ids,
        }


def write_response(payload: dict) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def main() -> None:
    args = parse_args()
    try:
        cache = ModelCache(args.token_map, args.id_map)
    except Exception as exc:  # noqa: BLE001 - report any startup failure to the parent
        write_response({"status": "error", "message": f"init failed: {exc}"})
        return

    # Handshake: tell the parent the worker is up and the protocol is live.
    write_response({"status": "ready"})
    log("ready, waiting for requests")

    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError as exc:
            write_response({"status": "error", "message": f"bad json: {exc}"})
            continue

        cmd = request.get("cmd", "generate")
        if cmd == "shutdown":
            log("shutdown requested")
            break
        if cmd == "ping":
            write_response({"status": "ok", "pong": True})
            continue
        if cmd != "generate":
            write_response({"status": "error", "message": f"unknown cmd: {cmd}"})
            continue

        try:
            write_response(cache.generate(request))
        except Exception as exc:  # noqa: BLE001 - keep the worker alive across errors
            log(f"generation error: {exc}")
            write_response({"status": "error", "message": str(exc)})


if __name__ == "__main__":
    main()
