#pragma once

#include <string>
#include "../config.hpp"

int obliczOdleglosc(std::string s1, std::string s2);

bool czyBladDopuszczalny(std::string slowo1, std::string slowo2, float prog = LEVENSHTEIN_PROG);
