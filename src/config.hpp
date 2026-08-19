#pragma once

constexpr const char* APP_NAME = "Retinere";

// Sciezki plikow danych (wzgledem katalogu roboczego; nadpisywalne przez g_sciezkaTalii)
constexpr const char* PLIK_TALII   = "decks/deck.json";
constexpr const char* PLIK_KONFIGURACJI = "data/config.txt";
constexpr const char* PLIK_SERII = "data/streak.txt";
constexpr const char* PLIK_DZIENNY  = "data/daily.txt";  // cel dnia + dzienny postep

// Progi tolerancji literowek (0.0-1.0)
constexpr float LEVENSHTEIN_DOKLADNY    = 0.0f;   // tylko idealne dopasowanie
constexpr float LEVENSHTEIN_PROG = 0.2f;   // tryb normalny: maks. 20% roznic
constexpr float LEVENSHTEIN_ULGOWY = 0.35f;  // tryb wyluzowany: maks. 35% roznic

// Tryby tolerancji — przechowywane w config.txt (linia 2)
constexpr int TRYB_DOKLADNY = 0;
constexpr int TRYB_NORMALNY = 1;
constexpr int TRYB_ULGOWY = 2;

// Domyslny dzienny cel powtorkowy
constexpr int DOMYSLNY_CEL_DZIENNY = 10;
