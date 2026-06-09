# Struktura projektu `kilter_cpp_gui`

Natywna aplikacja GTK4 do generowania tras Kilter (offline). Kod źródłowy jest podzielony na moduły z nagłówkami w `include/` i implementacjami w `src/`.

## Drzewo katalogów

```
kilter_cpp_gui/
├── CMakeLists.txt          # konfiguracja CMake (GTK4, SQLite3)
├── README.md               # dokumentacja funkcjonalna
├── STRUCTURE.md            # ten plik — opis struktury kodu
├── run.sh                  # skrypt budowania i uruchomienia
├── build/                  # katalog buildu CMake (generowany)
├── data/                   # lokalna baza SQLite (tworzona przy starcie)
│   └── saved_routes.db
├── include/                # nagłówki publiczne modułów
│   ├── types.hpp
│   ├── generator_process.hpp
│   ├── json_util.hpp
│   ├── util.hpp
│   ├── route.hpp
│   ├── board_db.hpp
│   ├── results_db.hpp
│   ├── token_loader.hpp
│   └── ui.hpp
└── src/                    # implementacje
    ├── main.cpp
    ├── generator_process.cpp
    ├── json_util.cpp
    ├── util.cpp
    ├── route.cpp
    ├── board_db.cpp
    ├── results_db.cpp
    ├── token_loader.cpp
    └── ui.cpp
```

## Moduły

| Plik | Odpowiedzialność |
|------|------------------|
| `types.hpp` | Struktury danych: `Hole`, `SavedRoute`, `AppState`, enum `RouteRole` |
| `generator_process` | Długożyjący worker Pythona — IPC przez `fork()` + pipe, protokół JSON |
| `json_util` | Prosta ekstrakcja pól string/array z odpowiedzi JSON workera |
| `util` | Funkcje pomocnicze: `trim`, parsowanie list Pythona, `join`, listowanie checkpointów, `popen`, znacznik czasu |
| `route` | Parsowanie tokenów tras (`pXXX` / `rYY`), role chwytów, stosowanie trasy do stanu |
| `board_db` | Mapowanie layout token → `product_size_id`, odczyt pozycji dziurek z `boardlib_data/kilter.sqlite3` |
| `results_db` | Lokalna baza `data/saved_routes.db` — inicjalizacja, zapis i odczyt historii tras |
| `token_loader` | Wczytanie tokenów layout/grade z `dataset/token_to_id.json` |
| `ui` | Interfejs GTK4: rysowanie planszy (Cairo), handlery przycisków, budowa okna |
| `main.cpp` | Inicjalizacja ścieżek, wczytanie danych startowych, uruchomienie `GtkApplication` |

## Zależności między modułami

```
main.cpp
  ├── types, board_db, results_db, token_loader, ui, util

ui.cpp
  ├── types, board_db, json_util, results_db, route, util

route.cpp
  ├── types, util

board_db.cpp / results_db.cpp / token_loader.cpp
  └── types (+ sqlite3 / stdlib)

generator_process.cpp
  └── (standalone — tylko POSIX IPC)
```

## Zewnętrzne zależności (ścieżki względem katalogu głównego repozytorium)

Aplikację należy uruchamiać z **roota repozytorium** — `main.cpp` zakłada względne ścieżki:

| Zasób | Ścieżka |
|-------|--------|
| Baza dziurek (read-only) | `boardlib_data/kilter.sqlite3` |
| Worker Pythona | `kilter_dl/generate_server.py` |
| Interpreter | `kilter_dl/.venv/bin/python` |
| Checkpointy | `kilter_dl/checkpoints/*.pt` |
| Mapy tokenów | `dataset/token_to_id.json`, `dataset/id_to_token.json` |
| Dataset (losowa trasa) | `dataset/dataset/single_board.json` |
| Wyniki użytkownika | `kilter_cpp_gui/data/saved_routes.db` |

## Budowanie i uruchomienie

```bash
# ze skryptu (zalecane)
./kilter_cpp_gui/run.sh

# tylko build
./kilter_cpp_gui/run.sh build

# tylko uruchomienie (po wcześniejszym buildzie)
./kilter_cpp_gui/run.sh run
```

Ręcznie:

```bash
cmake -S kilter_cpp_gui -B kilter_cpp_gui/build
cmake --build kilter_cpp_gui/build -j
./kilter_cpp_gui/build/kilter_gui   # z katalogu głównego repozytorium
```
