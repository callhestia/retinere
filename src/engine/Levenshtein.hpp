#pragma once

#include <string>
#include "../config.hpp"

int obliczOdleglosc(const std::string& s1, const std::string& s2);

bool czyBladDopuszczalny(const std::string& slowo1, const std::string& slowo2, float prog = LEVENSHTEIN_PROG);

