#pragma once
#include <string>

enum class TrybNauki { RECZNY, AUTOMATYCZNY };

struct Fiszka {
    std::string pytanie;
    std::string odpowiedz;
    double poziomTrudnosci = 2.5;
    std::string etykieta;
};