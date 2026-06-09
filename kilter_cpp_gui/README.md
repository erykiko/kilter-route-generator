# Kilter C++ GUI (Offline)

This app is a native C++ GTK4 GUI for the trained Kilter route-generation model.
It:
- loads Kilter hole positions from local `boardlib_data/kilter.sqlite3`,
- talks to a **persistent Python inference worker** over inter-process
  communication (IPC) to generate routes,
- renders generated route holds (start/middle/finish/foot) on a board,
- lets the user **rate a route 1–5 and save it to a local database** after a
  confirmation dialog.

No online API is required.

## First-time setup (after `git clone`)

Large files are **not** stored in the repository (GitHub 100 MB file limit).
Run once from the project root:

```bash
./setup_gui.sh
```

This creates `kilter_dl/.venv`, downloads `boardlib_data/kilter.sqlite3` via
[boardlib](https://github.com/boardlib/boardlib), and checks that the checkpoint
and dataset files from git are present.

## Build

From project root:

```bash
cmake -S kilter_cpp_gui -B kilter_cpp_gui/build
cmake --build kilter_cpp_gui/build -j
```

## Run

Run from project root (important for relative paths):

```bash
./kilter_cpp_gui/build/kilter_gui
```

## Inter-process communication (route generator)

Instead of spawning a fresh Python interpreter on every click, the GUI launches
one long-lived worker process and keeps it alive:

- Worker: `kilter_dl/generate_server.py`, launched once via `fork()` + `execl()`.
- Channel: a pair of pipes wired to the worker's `stdin`/`stdout`
  (`GeneratorProcess` in `src/main.cpp`).
- Protocol: line-delimited JSON, one request per line, one response per line.
  - Handshake: worker emits `{"status":"ready"}` once it has imported torch.
  - Request: `{"cmd":"generate","checkpoint":"...","layout_id":1270,"grade_id":1239,
    "greedy":false,"temperature":1.15,"top_k":40,"top_p":0.95,"repetition_penalty":1.1}`
  - Response: `{"status":"ok","tokens":[...],"hole_ids":[...],"token_ids":[...]}`
    or `{"status":"error","message":"..."}`
- The worker caches one model per checkpoint path, so torch import and weight
  loading happen only once. Diagnostics go to `stderr` and never corrupt the
  channel. On window close the GUI sends `{"cmd":"shutdown"}` and reaps the child.

You can test the worker independently:

```bash
printf '{"cmd":"generate","checkpoint":"kilter_dl/checkpoints/kilter_gen.pt","layout_id":1270,"grade_id":1239,"greedy":true}\n{"cmd":"shutdown"}\n' \
  | kilter_dl/.venv/bin/python kilter_dl/generate_server.py
```

## Local database (saved routes + rating)

A separate, writable SQLite database stores routes the user chooses to keep:

- File: `kilter_cpp_gui/data/saved_routes.db` (created automatically on startup).
- Table `saved_routes`:
  `id, created_at, layout_id, grade_id, source, rating, hole_count, tokens, hole_ids`.
- Workflow:
  1. Generate a route (model) or load a random one (dataset).
  2. Set **Your rating (1–5)** with the spin button.
  3. Click **Save route + rating to DB** and **confirm** in the dialog
     ("zapis po zatwierdzeniu").
  4. The "Recently saved routes" panel reads the latest entries back from the DB.

The board database (`boardlib_data/kilter.sqlite3`) is only read; all writes go
to the dedicated `saved_routes.db`.

## GUI inputs

- `Board`: layout token id (example: `1270`)
- `Grade`: grade token id (example: `1239`)
- `Checkpoint`: trained checkpoint (`.pt`) from `kilter_dl/checkpoints`
- Advanced: temperature / top-k / top-p / repetition penalty / greedy decoding
- `Your rating (1–5)`: rating attached to the route when saved

## Notes

Ensure:
- `boardlib_data/kilter.sqlite3` exists
- `kilter_dl/.venv` exists with dependencies (torch)
- at least one checkpoint exists under `kilter_dl/checkpoints`
