#include <chrono>
#include <clocale>
#include <iostream>
#include <locale>
#ifdef _WIN32
#include <windows.h>
#endif
#include "ui/menu.hpp"
#include "storage/file_manager.hpp"
#include "config.hpp"

static void ustawLocalePolskie() {
    std::setlocale(LC_ALL, "");

    try {
        std::locale::global(std::locale("pl_PL.UTF-8"));
    } catch (...) {
        try {
            std::locale::global(std::locale(""));
        } catch (...) {
            // jeśli lokalizacja nie jest dostępna, kontynuuj z domyślną
        }
    }

    std::cout.imbue(std::locale());
    std::cin.imbue(std::locale());

#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

int main(int argc, char* argv[]) {
    // -----------------------------------------------------------------------
    // Parsowanie flag wiersza polecen
    // -----------------------------------------------------------------------
    bool trybStatystyk = false;
    std::string nadpisanyTryb = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--deck" && i + 1 < argc) {
            g_sciezkaTalii = argv[++i];
        } else if (arg == "--mode" && i + 1 < argc) {
            nadpisanyTryb = argv[++i];
            if (nadpisanyTryb != "auto" && nadpisanyTryb != "manual") {
                std::cerr << "Blad: --mode przyjmuje 'auto' lub 'manual'\n";
                return 1;
            }
        } else if (arg == "--stats") {
            trybStatystyk = true;
        } else {
            std::cerr << "Nieznana opcja: " << arg << "\n";
            std::cerr << "Uzycie: retinere [--deck <plik>] [--mode auto|manual] [--stats]\n";
            return 1;
        }
    }

    ustawLocalePolskie();
    auto fiszki = wczytajTalie();

    if (trybStatystyk) {
        wyswietlStatystyki(fiszki, false);
        return 0;
    }

    int trybKonfiguracji = wczytajKonfiguracje();
    if (!nadpisanyTryb.empty())
        trybKonfiguracji = (nadpisanyTryb == "auto") ? 1 : 0;
    TrybNauki tryb = trybKonfiguracji == 1 ? TrybNauki::AUTOMATYCZNY : TrybNauki::RECZNY;

    int trybLiterowek = wczytajTrybLiterowek();

    // Oblicz dzisiejszy dzien (liczba dni od epoki Unix)
    using days = std::chrono::duration<int, std::ratio<86400>>;
    int dzisiaj = (int)std::chrono::duration_cast<days>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    int celDzienny      = wczytajCelDzienny();
    int fiszkiDzisiaj = wczytajPostepDzienny(dzisiaj);

    int ostatniDzien = 0, seria = 0;
    wczytajDaneSerii(ostatniDzien, seria);

    if (ostatniDzien == 0) {
        seria = 1;                  // pierwsze uruchomienie
    } else if (dzisiaj == ostatniDzien) {
        // ta sama sesja w ciagu dnia — seria bez zmian
    } else if (dzisiaj == ostatniDzien + 1) {
        seria++;                    // kolejny dzien z rzedu
    } else {
        seria = 1;                  // przerwa — reset
    }
    zapiszDaneSerii(dzisiaj, seria);

    uruchomMenu(fiszki, tryb, seria, trybLiterowek, celDzienny, fiszkiDzisiaj);
    zapiszTalie(fiszki);
    zapiszPostepDzienny(dzisiaj, fiszkiDzisiaj);

    std::cout << "Koniec programu. Deck zapisany do " << g_sciezkaTalii << "\n";
    return 0;
}