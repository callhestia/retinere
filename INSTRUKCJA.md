# Instrukcja uruchomienia — Retinere

## Wymagania

- Linux (lub WSL / macOS)
- `g++` z obsługą C++17
- `cmake` >= 3.10
- `make`

Sprawdź dostępność:
```bash
g++ --version
cmake --version
```

---

## Budowanie

```bash
git clone https://github.com/callhestia/retinere.git
cd retinere

cmake -B build -S .
cmake --build build
```

Skompilowany plik wykonywalny pojawi się w `build/retinere`.

---

## Uruchamianie

Program musi być uruchamiany z katalogu zawierającego pliki danych.

```bash
cd build
./retinere
```

Przy pierwszym uruchomieniu program automatycznie tworzy pusty plik `data/deck.txt`.

### Opcje wiersza poleceń

| Flaga | Opis |
|---|---|
| `--deck <sciezka>` | Użyj innego pliku z fiszkami |
| `--auto` | Wymuś tryb wpisywania (Levenshtein) |
| `--manual` | Wymuś tryb samooceny (0–5) |
| `--stats` | Wyświetl statystyki i wyjdź |

Przykład:
```bash
./retinere --deck ../decks/example.txt --auto
```

---

## Pliki danych

Wszystkie pliki danych są odczytywane względem bieżącego katalogu roboczego:

| Plik | Zawartość |
|---|---|
| `decks/deck.txt` | Fiszki (format: `pytanie\|odpowiedź\|poziomTrudnosci\|etykieta`) |
| `data/config.txt` | Tryb nauki (linia 1) i tolerancja literówek (linia 2) |
| `data/streak.txt` | Ostatni dzień nauki i długość serii |
| `data/daily.txt` | Cel dzienny, ostatni dzień i postęp |

---

## Testy

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Lub bezpośrednio:
```bash
./build/tests/test_engine
```
