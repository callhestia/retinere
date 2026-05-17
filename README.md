<div align="center">
  <img src="assets/Retinere.svg" alt="Retinere Logo" width="500">
  
  <h1>Retinere</h1>
</div>
Retinere to minimalistyczna aplikacja terminalowa do nauki z wykorzystaniem algorytmu powtórek rozłożonych w czasie. Projekt napisany na zajęcia z programowania w czystym C++17, stawiający na szybkość, brak zewnętrznych zależności i prostotę. Działa całkowicie z poziomu wiersza poleceń, oferując czas uruchomienia poniżej 100ms.

## Kluczowe funkcje

Program zarządza procesem nauki poprzez automatyczne planowanie kolejności i częstotliwości powtórek, śledząc przy tym dzienne postępy oraz serie nauki (streak). Fiszki przechowywane są w formacie zwykłego tekstu `.txt`, co ułatwia ich ręczną edycję i przenoszenie. (Możliwe rozwiązanie - pliki w formacie .JSON)

Użytkownik ma do dyspozycji dwa tryby nauki:
1. **Samoocena** — klasyczny model, w którym użytkownik sam ocenia stopień znajomości odpowiedzi w skali od 0 do 5.
2. **Wpisywanie** — aktywny tryb weryfikacji. Program analizuje wpisaną odpowiedź wykorzystując odległość edycyjną (algorytm Levenshteina), aby tolerować drobne literówki.

Aplikacja pozwala również na pełne zarządzanie biblioteką: dodawanie, przeglądanie, wyszukiwanie i filtrowanie fiszek po etykietach, a każda sesja kończy się podsumowaniem statystyk (czas, poprawność, aktualna seria).

## Pod maską (Algorytmy)

**Algorytm SM-2 i wagi fiszek**
Każda fiszka posiada swój współczynnik łatwości (domyślnie 2.5). Poprawna odpowiedź zwiększa ten współczynnik, odsuwając kolejną powtórkę w czasie. Błędna odpowiedź obniża go (od -0.15 do -0.30 w zależności od stopnia błędu), co skutkuje szybszym powrotem pytania. Zamiast sztywnego harmonogramu kalendarzowego zastosowano system wagowy — fiszki o niższym współczynniku otrzymują wyższą wagę i są częściej losowane w trakcie bieżącej sesji.

**Analiza tekstu (Levenshtein)**
W trybie wpisywania zaimplementowano trzy poziomy tolerancji dla błędów:
* **Dokładny** — wymaga 100% zgodności znaków.
* **Normalny** — dopuszcza do 20% różnic.
* **Ulgowy** — dopuszcza do 35% różnic.

## Kompilacja i uruchomienie

Jako projekt w czystym C++17, Retinere kompiluje się bez problemu na systemach Linux oraz macOS. Poniżej znajduje się proces budowania aplikacji z wykorzystaniem systemu CMake.

```bash
git clone https://github.com/callhestia/retinere.git
cd retinere
mkdir build
cd build
cmake ..
make
./retinere
```

## Struktura projektu

Architektura opiera się na wyraźnym podziale odpowiedzialności:

```text
src/
  config.hpp          — stałe konfiguracyjne
  main.cpp            — punkt wejścia, parsowanie flag CLI
  engine/             — logika: SM-2, Levenshtein, system ocen
  storage/            — operacje I/O: odczyt/zapis fiszek, configu i postępów
  models/             — struktury danych: Fiszka, TrybNauki
  ui/                 — interfejs terminalowy i formatowanie wyjścia
tests/
  test_engine.cpp     — testy jednostkowe dla algorytmów
decks/
  deck.txt            — domyślna talia (plan rozbicia na więcej plików)
```

## Zespół twórców

Projekt rozwijany przez trzy osoby z wyraźnym podziałem na moduły:

| Osoba | Moduł | Zakres odpowiedzialności |
|---|---|---|
| **Franciszek** | `src/engine/` | Implementacja SM-2, odległości Levenshteina, logika wyboru i oceniania fiszek. Kontrola wersji przez GH i koordynacja projektu.|
| **Dawid** | `src/storage/` | System plików i danych, zapis i odczyt konfiguracji oraz statystyk. |
| **Mikołaj** | `src/ui/` | Zarządzanie menu, interakcja z użytkownikiem, prezentacja danych i statystyk w terminalu. |

## Plany rozwoju (Roadmap)

W przyszłości można pomyśleć o migracji modelu pamięci na nowocześniejszy algorytm FSRS oraz implementacja synchronizacji talii przez sieć. Architektura silnika (oddzielenie od UI) pozwala w przyszłości na łatwe dobudowanie interfejsu webowego, wsparcia dla multimediów w fiszkach oraz trybu współdzielenia talii między wieloma użytkownikami.
