#pragma once

#include "../models/flashcard.hpp"
#include "../config.hpp"
#include <string>
#include <vector>

Fiszka krokNauki(Fiszka fiszka, TrybNauki tryb, std::string odpowiedzUzytkownika, int ocena = 0);

double zaplanujPowtorke(int ocena, double poprzedniWspolczynnik);

int obliczStatusOdpowiedzi(std::string odpowiedz, Fiszka fiszka, float prog);

float progOdTrybu(int trybLiterowek);

double obliczNowaWage(double aktualnaWaga, std::vector<double> wagi);

int wybierzNastepna(std::vector<double> wagi, int poprzedniIndeks = -1);