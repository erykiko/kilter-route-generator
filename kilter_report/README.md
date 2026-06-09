# kilter_report

Pomocnicza biblioteka generująca wektorowe wykresy (PDF) na podstawie logów
eksperymentów projektu `kilter_dl`. Wykresy są używane w raporcie
LaTeX (`report/raport.tex`).

## Co generuje

Wczytuje:

- `kilter_dl/logs/exp_*/metrics.csv` oraz `run_summary.json` — krzywe
  uczenia i podsumowania konfiguracji,
- `kilter_dl/samples/exp_*/sample_<layout>_<grade>.json` — wyniki
  generacji w różnych konfiguracjach dekodowania.

Zapisuje do `report/figures/`:

- krzywe `train_nll` / `val_nll` w funkcji epoki dla każdej grupy
  eksperymentów (lr, capacity, dropout, batch_size, layers),
- zbiorczy słupkowy wykres najlepszego `val_nll` dla wszystkich
  scenariuszy treningowych,
- zbiorczy słupkowy wykres średniego czasu epoki,
- statystyki dla scenariuszy generacji (długość trasy, udział
  unikalnych chwytów) oraz osobne porównania dla parametrów
  temperatury, top-k, top-p i repetition penalty.

## Uruchomienie

```bash
./kilter_report/run_plots.sh
# lub bezpośrednio:
python -m kilter_report --logs-dir kilter_dl/logs \
                        --samples-dir kilter_dl/samples \
                        --output-dir report/figures
```

Pierwsze uruchomienie skryptu instaluje izolowane środowisko
`kilter_report/.venv` (Python + matplotlib).

## Struktura

```
kilter_report/
├── __init__.py
├── __main__.py     # CLI: python -m kilter_report …
├── data.py         # ładowanie metryk i próbek
├── plots.py        # wszystkie funkcje rysujące (matplotlib → PDF)
├── requirements.txt
├── run_plots.sh    # auto-bootstrap venv + uruchomienie
└── README.md
```
