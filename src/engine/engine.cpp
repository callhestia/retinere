#include "engine.hpp"
#include "Levenshtein.hpp"
#include <algorithm>
#include <random>
#include <vector>
using namespace std;

int obliczStatusOdpowiedzi(string odpowiedz, Fiszka fiszka, float prog) {
    
    if (odpowiedz == fiszka.odpowiedz)
    {
        return 0;
    }

    if (czyBladDopuszczalny(odpowiedz, fiszka.odpowiedz, prog)){
        return 1;
    }

    return 2;
}

float progOdTrybu(int trybLiterowek) {
    if (trybLiterowek == TRYB_DOKLADNY) return LEVENSHTEIN_DOKLADNY;
    if (trybLiterowek == TRYB_ULGOWY)   return LEVENSHTEIN_ULGOWY;
    return LEVENSHTEIN_PROG;
}

static Fiszka aktualizujFiszke(Fiszka fiszka, int ocena) {
    fiszka.poziomTrudnosci = zaplanujPowtorke(ocena, fiszka.poziomTrudnosci);
    return fiszka;
}

static int przeliczBladNaOcene(string odpowiedzUzytkownika, string poprawnaOdpowiedz) {
    int maxDlugosc = static_cast<int>(max(odpowiedzUzytkownika.length(), poprawnaOdpowiedz.length()));
    
    if (maxDlugosc == 0) 
    {
        return 5;
    }

    int odleglosc = obliczOdleglosc(odpowiedzUzytkownika, poprawnaOdpowiedz);
    double wspolczynnikBledu = static_cast<double>(odleglosc) / static_cast<double>(maxDlugosc);

    if (wspolczynnikBledu == 0.0)  return 5;
    if (wspolczynnikBledu <= 0.20) return 4;
    if (wspolczynnikBledu <= 0.35) return 3;
    if (wspolczynnikBledu <= 0.50) return 2;
    if (wspolczynnikBledu <= 0.70) return 1;
    return 0;                                // czyli odpowiedz calkowicie bledna
}

static int ograniczOcene(int ocena) {
    if (ocena < 0) return 0;
    if (ocena > 5) return 5;
    return ocena;
}

Fiszka krokNauki(Fiszka fiszka, TrybNauki tryb, string odpowiedzUzytkownika, int ocena) {
    int finalnaOcena = 0;

    switch (tryb) {
        case TrybNauki::RECZNY:
            finalnaOcena = ograniczOcene(ocena);
            break;

        case TrybNauki::AUTOMATYCZNY:
            finalnaOcena = przeliczBladNaOcene(odpowiedzUzytkownika, fiszka.odpowiedz);
            break;
    }

    return aktualizujFiszke(fiszka, finalnaOcena);
}

double obliczNowaWage(double aktualnaWaga, vector<double> wagi) {
    double sumaWag = 0.0;
    int liczbaAktywnych = 0;

    for (double w : wagi) if (w > 0) { 
        sumaWag += w; liczbaAktywnych++; 
    }

    double sredniaWaga = liczbaAktywnych > 0 ? sumaWag / liczbaAktywnych : 1.0;

    return min(aktualnaWaga * 2.0, sredniaWaga * 3.0);
}

int wybierzNastepna(vector<double> wagi, int poprzedniIndeks) {

    double sumaWag = 0.0;

    for (double w : wagi) sumaWag += w;

        if (sumaWag <= 0.0) {
            return -1;
        }

    double sumaWagBezPoprzedniej = sumaWag - (poprzedniIndeks >= 0 ? wagi[poprzedniIndeks] : 0.0);

    bool pominPoprzednia = (poprzedniIndeks >= 0) && (sumaWagBezPoprzedniej > 0.0);

    static mt19937 generator(random_device{}());

    uniform_real_distribution<double> rozklad(0.0, pominPoprzednia ? sumaWagBezPoprzedniej : sumaWag);

    double wartoscLosowa = rozklad(generator);

    double sumaSkumulowana = 0.0;

    for (int i = 0; i < (int)wagi.size(); ++i) {

        if (pominPoprzednia && i == poprzedniIndeks) continue;
        sumaSkumulowana += wagi[i];
        if (wartoscLosowa < sumaSkumulowana) return i;

    }

    for (int i = (int)wagi.size() - 1; i >= 0; --i) {

        if (pominPoprzednia && i == poprzedniIndeks) continue;
        if (wagi[i] > 0.0) return i;
    }


    return (int)wagi.size() - 1;
}
