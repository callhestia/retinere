// menu.hpp
#pragma once
#include "../storage/Flashcard.hpp"
#include <vector>

// Glowna petla interfejsu — wywolaj z main.cpp
// celDzienny: cel dzienny (0 = brak); fiszkiDzisiaj: licznik sesji (in/out)
void uruchomMenu(std::vector<Fiszka>& fiszki, TrybNauki& trybNauki, int seria, int& trybLiterowek,
                 int celDzienny, int& fiszkiDzisiaj);

// Statystyki talii. czekaj=false pomija oczekiwanie na Enter (np. flaga --stats)
void wyswietlStatystyki(const std::vector<Fiszka>& fiszki, bool czekaj = true);