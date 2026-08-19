#include "engine.hpp"       // zasysa struktury danych z "../models/flashcard.hpp" i ustawienia z "config.hpp".
#include "Levenshtein.hpp"  // Prowadzi do definicji funkcji w pliku Levenshtein.cpp.
#include <algorithm>        // min(), max()
#include <random>           // generator losowy mt19937.
#include <vector>

using namespace std;

// Normalize: lowercase + strip leading/trailing whitespace
static string znormalizuj(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    string r = s.substr(a, b - a + 1);
    transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return tolower(c); });
    return r;
}

// & przeciw redundancji
int obliczStatusOdpowiedzi(const string& odpowiedz, const Fiszka& fiszka, float prog) {
    const string odp = znormalizuj(odpowiedz);
    const string ans = znormalizuj(fiszka.odpowiedz);

    if (odp == ans) return 0;
    if (czyBladDopuszczalny(odp, ans, prog)) return 1;
    return 2;
}

// consty wczytywane z config.hpp.
float progOdTrybu(int trybLiterowek) {
    if (trybLiterowek == TRYB_DOKLADNY) return LEVENSHTEIN_DOKLADNY;
    if (trybLiterowek == TRYB_ULGOWY)   return LEVENSHTEIN_ULGOWY;
    
    return LEVENSHTEIN_PROG; // def
}

// Wczytuje ocene, wylicza nowy interwal i zapisuje fiszke
// zP funkcja algorytmu SM-2

static Fiszka aktualizujFiszke(Fiszka fiszka, int ocena) {
    fiszka.poziomTrudnosci = zaplanujPowtorke(ocena, fiszka.poziomTrudnosci);
    return fiszka;
}

static int przeliczBladNaOcene(const string& odpowiedzUzytkownika, const string& poprawnaOdpowiedz) {
    const string odp = znormalizuj(odpowiedzUzytkownika);
    const string ans = znormalizuj(poprawnaOdpowiedz);
    int maxDlugosc = static_cast<int>(max(odp.length(), ans.length()));
    
    if (maxDlugosc == 0) return 5;

    int odleglosc = obliczOdleglosc(odp, ans);
    double wspolczynnikBledu = static_cast<double>(odleglosc) / static_cast<double>(maxDlugosc);

    // Przeliczanie na oceny
    if (wspolczynnikBledu == 0.0)  return 5;
    if (wspolczynnikBledu <= 0.20 && wspolczynnikBledu > 0.0) return 4;
    if (wspolczynnikBledu > 0.20 && wspolczynnikBledu <= 0.35) return 3;
    if (wspolczynnikBledu > 0.35 && wspolczynnikBledu <= 0.50) return 2;
    if (wspolczynnikBledu > 0.50 && wspolczynnikBledu <= 0.70) return 1;
    
    return 0; // def bledna
}

// zapobiegawczo przeciw bledom ui
static int ograniczOcene(int ocena) {
    if (ocena < 0) {
        return 0;
    }
    if (ocena > 5) {
        return 5;
    }
    return ocena;
}


// enum jest zdefiniowany w pliku ../models/flashcard.hpp.

Fiszka krokNauki(const Fiszka& fiszka, TrybNauki tryb, const string& odpowiedzUzytkownika, int ocena) {
    int finalnaOcena = 0;

    if (tryb == TrybNauki::RECZNY) {
        finalnaOcena = ograniczOcene(ocena);
    } 
    else if (tryb == TrybNauki::AUTOMATYCZNY) {
        finalnaOcena = przeliczBladNaOcene(odpowiedzUzytkownika, fiszka.odpowiedz);
    }

    return aktualizujFiszke(fiszka, finalnaOcena);
}

// Rozporzadzenie ministra ws. przeciwdzialania monopolom fiszek
double obliczNowaWage(double aktualnaWaga, const vector<double>& wagi) {
    double sumaWag = 0.0;
    int liczbaAktywnych = 0;

    for (double w : wagi) {
        if (w > 0.0) {
            sumaWag += w;
            liczbaAktywnych++;
        }
    }

    // Nie dziel przez zero...
    double sredniaWaga = 1.0;
    if (liczbaAktywnych > 0) {
        sredniaWaga = sumaWag / liczbaAktywnych;
    }

    double podwojonaWaga = aktualnaWaga * 2.0;
    double limitWzrostu = sredniaWaga * 3.0;

    if (podwojonaWaga < limitWzrostu) {
        return podwojonaWaga;
    } else {
        return limitWzrostu;
    }
}

// algorytm gamba
int wybierzNastepna(const vector<double>& wagi, int poprzedniIndeks) {
    
    double sumaWag = 0.0;
    for (double w : wagi) {
        sumaWag += w;
    }

    if (sumaWag <= 0.0) {
        return -1;
    }

    double sumaWagBezPoprzedniej = sumaWag;
    if (poprzedniIndeks >= 0) {
        sumaWagBezPoprzedniej -= wagi[poprzedniIndeks];
    }

    bool pominPoprzednia = false;
    if (poprzedniIndeks >= 0 && sumaWagBezPoprzedniej > 0.0) {
        pominPoprzednia = true;
    }

    // Utworzony tylko raz (static), żeby nie zacinał się na tym samym ziarnie losowości
    static mt19937 generator(std::random_device{}());

    double gornaGranica = sumaWag;
    if (pominPoprzednia) {
        gornaGranica = sumaWagBezPoprzedniej;
    }

    // Zamiana dużych liczb z generatora na ułamek w wyznaczonym przedziale
    uniform_real_distribution<double> rozklad(0.0, gornaGranica);
    double strzalkaRuletki = rozklad(generator);

    double sumaLaczna = 0.0;
    for (int i = 0; i < wagi.size(); ++i) {
        
        if (pominPoprzednia && i == poprzedniIndeks) {
            continue; 
        }

        sumaLaczna += wagi[i];
        if (strzalkaRuletki < sumaLaczna) {
            return i;
        }
    }

    // Awaryjne
    for (int i = wagi.size() - 1; i >= 0; --i) {
        if (pominPoprzednia && i == poprzedniIndeks) {
            continue;
        }
        if (wagi[i] > 0.0) {
            return i;
        }
    }

    return wagi.size() - 1;
}