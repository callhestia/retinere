#pragma once

#include "../models/flashcard.hpp"
#include "../config.hpp"
#include <string>
#include <vector>

// Przyjmujemy duże obiekty przez stałą referencję, aby nie zapychać pamięci kopiami.
Fiszka krokNauki(const Fiszka& fiszka, TrybNauki tryb, const std::string& odpowiedzUzytkownika, int ocena = 0);

double zaplanujPowtorke(int ocena, double poprzedniWspolczynnik);

int obliczStatusOdpowiedzi(const std::string& odpowiedz, const Fiszka& fiszka, float prog);

float progOdTrybu(int trybLiterowek);

double obliczNowaWage(double aktualnaWaga, const std::vector<double>& wagi);

int wybierzNastepna(const std::vector<double>& wagi, int poprzedniIndeks = -1);