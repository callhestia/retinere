// menu.cpp — interfejs konsolowy, Mikołaj
#include "menu.hpp"
#include "../engine/engine.hpp"
#include "../engine/Levenshtein.hpp"
#include "../storage/file_manager.hpp"
#include "../storage/Flashcard.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <set>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

// Odczytuje jeden znak bez potrzeby naciskania Enter
static int getchRaw() {
#ifdef _WIN32
    return _getch();
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
#endif
}

// -----------------------------------------------------------------------
// Funkcje pomocnicze UI
// -----------------------------------------------------------------------

void czyscEkran() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void rysujLinie() {
    cout << "\n------------------------------------------\n";
}

void czekajNaEnter() {
    cout << "\n[Nacisnij Enter, aby kontynuowac...]";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

static int czytajInt() {
    string line;
    getline(cin, line);
    try {
        return stoi(line);
    } catch (...) {
        return -1;
    }
}

enum class WynikSesji { NORMALNY, POWROT_DO_MENU, ZAKONCZ };

struct StatystykiSesji {
    WynikSesji wynik    = WynikSesji::NORMALNY;
    int przejrzano = 0;   // fiszek ukonczone w sesji
    int poprawne   = 0;
    int literowki  = 0;
    int bledy      = 0;
    int sekundy    = 0;
};

StatystykiSesji trybWpisywania(vector<Fiszka*>& fiszki, float prog) {
    StatystykiSesji stats;
    if (fiszki.empty()) {
        cout << "\nBrak fiszek do powtorki na dzis! Wróc jutro.\n";
        czekajNaEnter();
        return stats;
    }

    czyscEkran();
    cout << "=== TRYB WPISYWANIA ===\n";
    cout << "Fiszek do przejrzenia: " << fiszki.size() << "\n";
    cout << "Wpisuj odpowiedzi - algorytm sam oceni literowki.\n";
    czekajNaEnter();

    auto t0 = chrono::steady_clock::now();
    int poprawne = 0, literowki = 0, bledy = 0;

    // Wagi: nowa fiszka startuje z 1.0; rosna po bledzie, zeruja sie po poprawnej odpowiedzi
    vector<double> wagi(fiszki.size(), 1.0);
    int pozostalo = (int)fiszki.size();
    int lastIdx = -1;

    while (pozostalo > 0) {
        int idx = wybierzNastepna(wagi, lastIdx);
        if (idx < 0) break; // bezpieczenstwo: nie powinno wystapic
        lastIdx = idx;
        Fiszka* fp = fiszki[idx];

        czyscEkran();
        rysujLinie();
        cout << "PYTANIE: " << fp->pytanie << "\n";
        rysujLinie();
        cout << "Twoja odpowiedz: ";

        string odpowiedz;
        getline(cin, odpowiedz);

        while (!odpowiedz.empty() && odpowiedz.front() == ' ')
            odpowiedz.erase(odpowiedz.begin());
        while (!odpowiedz.empty() && odpowiedz.back() == ' ')
            odpowiedz.pop_back();

        if (odpowiedz == "kill") { stats.wynik = WynikSesji::ZAKONCZ; return stats; }
        if (odpowiedz == "menu") { stats.wynik = WynikSesji::POWROT_DO_MENU; return stats; }

        int status = obliczStatusOdpowiedzi(odpowiedz, *fp, prog);
        *fp = krokNauki(*fp, TrybNauki::AUTOMATYCZNY, odpowiedz);

        rysujLinie();
        if (status == 0) {
            cout << "  [BINGO!] Swietna robota.\n";
            poprawne++;
            pozostalo--;
            wagi[idx] = 0.0;
        } else if (status == 1) {
            cout << "  [PRAWIE!] Mala literowka \u2014 nie przejmuj sie.\n";
            cout << "  Poprawna odpowiedz: " << fp->odpowiedz << "\n";
            literowki++;
            pozostalo--;
            wagi[idx] = 0.0;
        } else {
            cout << "  [BLAD] Nastepnym razem bedzie lepiej.\n";
            cout << "  Poprawna odpowiedz: " << fp->odpowiedz << "\n";
            bledy++;
            wagi[idx] = obliczNowaWage(wagi[idx], wagi);
        }

        czekajNaEnter();
    }

    // Podsumowanie sesji
    auto t1 = chrono::steady_clock::now();
    stats.sekundy    = (int)chrono::duration_cast<chrono::seconds>(t1 - t0).count();
    stats.przejrzano = poprawne + literowki;
    stats.poprawne   = poprawne;
    stats.literowki  = literowki;
    stats.bledy      = bledy;
    czyscEkran();
    rysujLinie();
    cout << "  === KONIEC SESJI \u2014 WPISYWANIE ===\n\n";
    cout << "  Idealnie:  " << poprawne  << "  |  Literowki: " << literowki
         << "  |  Bledy: " << bledy << "\n";
    cout << "  Czas sesji: " << stats.sekundy << " s\n";
    rysujLinie();
    czekajNaEnter();
    return stats;
}

StatystykiSesji trybSamooceny(vector<Fiszka*>& fiszki) {
    StatystykiSesji stats;
    if (fiszki.empty()) {
        cout << "\nBrak fiszek do powtorki na dzis!\n";
        czekajNaEnter();
        return stats;
    }

    czyscEkran();
    cout << "=== TRYB SAMOOCENY ===\n";
    cout << "Fiszek do przejrzenia: " << fiszki.size() << "\n\n";
    cout << "Skala ocen:\n";
    cout << "  5 - Najlepiej\n";
    cout << "  4 - \n";
    cout << "  3 - \n";
    cout << "  2 - \n";
    cout << "  1 - \n";
    cout << "  0 - Najgorzej\n";
    czekajNaEnter();

    // Wagi: każda fiszka startuje z 1.0
    vector<double> wagi(fiszki.size(), 1.0);
    int pozostalo = (int)fiszki.size();
    int lastIdx = -1;

    auto t0 = chrono::steady_clock::now();
    while (pozostalo > 0) {
        int idx = wybierzNastepna(wagi, lastIdx);
        if (idx < 0) break; // bezpieczenstwo: nie powinno wystapic
        lastIdx = idx;
        Fiszka* fp = fiszki[idx];

        czyscEkran();
        rysujLinie();
        cout << "PYTANIE: " << fp->pytanie << "\n";
        rysujLinie();
        cout << endl<< "[Naciśnij Enter aby kontynuować]\n";
        
        string cmd = "";
        getline(cin, cmd);
        if (cmd == "kill") { stats.wynik = WynikSesji::ZAKONCZ; return stats; }
        if (cmd == "menu") { stats.wynik = WynikSesji::POWROT_DO_MENU; return stats; }

        cout << "\nODPOWIEDZ: " << fp->odpowiedz << "\n";
        rysujLinie();

        int ocena = -1;
        while (ocena < 0 || ocena > 5) {
            cout << "Jak Ci poszlo? (0-5): " << flush;
            int ch = getchRaw();
            if (ch >= '0' && ch <= '5') {
                ocena = ch - '0';
                cout << (char)ch << "\n";
            }
        }
        // Wyczysc bufor wejscia — użytkownik mógł nacisnąć Enter po cyfrze
#ifndef _WIN32
        tcflush(STDIN_FILENO, TCIFLUSH);
#endif
        cin.clear();
        cin.sync();

        *fp = krokNauki(*fp, TrybNauki::RECZNY, "", ocena);

        if (ocena >= 4) {
            wagi[idx] = 0.0;
            pozostalo--;
            stats.poprawne++;
            stats.przejrzano++;
        } else {
            wagi[idx] = obliczNowaWage(wagi[idx], wagi);
            stats.bledy++;
        }
    }

    auto t1 = chrono::steady_clock::now();
    stats.sekundy = (int)chrono::duration_cast<chrono::seconds>(t1 - t0).count();
    czyscEkran();
    rysujLinie();
    cout << "  === KONIEC SESJI \u2014 SAMOOCENA ===\n\n";
    cout << "  Poprawne (4-5):   " << stats.poprawne << "\n";
    cout << "  Do poprawy (0-3): " << stats.bledy    << "\n";
    cout << "  Czas sesji:       " << stats.sekundy  << " s\n";
    rysujLinie();
    czekajNaEnter();
    return stats;
}

// -----------------------------------------------------------------------
// Podmenu: dodawanie nowej fiszki ręcznie z klawiatury
// -----------------------------------------------------------------------

void dodajFiszke(vector<Fiszka>& fiszki) {
    czyscEkran();
    cout << "=== DODAJ FISZKI ===\n\n";
    cout << "Uwaga: nie uzywaj znaku | w tresci!\n";
    cout << "Zostaw pytanie puste, aby zakonczyc dodawanie.\n\n";

    int dodano = 0;
    while (true) {
        Fiszka nowaFiszka;

        cout << "Pytanie (Enter = koniec): ";
        getline(cin, nowaFiszka.pytanie);
        if (nowaFiszka.pytanie.empty()) break;

        cout << "Odpowiedz: ";
        getline(cin, nowaFiszka.odpowiedz);
        if (nowaFiszka.odpowiedz.empty()) {
            cout << "  Odpowiedz nie moze byc pusta. Pomijam te fiszke.\n\n";
            continue;
        }

        nowaFiszka.poziomTrudnosci = 2.5;

        cout << "Tag/kategoria (opcjonalnie, Enter aby pominac): ";
        getline(cin, nowaFiszka.etykieta);
        while (!nowaFiszka.etykieta.empty() && nowaFiszka.etykieta.front() == ' ')
            nowaFiszka.etykieta.erase(nowaFiszka.etykieta.begin());
        while (!nowaFiszka.etykieta.empty() && nowaFiszka.etykieta.back() == ' ')
            nowaFiszka.etykieta.pop_back();

        fiszki.push_back(nowaFiszka);
        dodano++;
        cout << "  [Dodano]\n\n";
    }

    if (dodano > 0) {
        zapiszTalie(fiszki);
        cout << "\n  Dodano " << dodano << " fiszek. Zapisano w " << g_sciezkaTalii << "\n";
    } else {
        cout << "\n  Nie dodano zadnych fiszek.\n";
    }
    czekajNaEnter();
}

// -----------------------------------------------------------------------
// Wybor kategorii (tagu) przed sesja nauki
// Zwraca wybrany etykieta lub pusty string (= wszystkie fiszki)
// -----------------------------------------------------------------------

static string wybierzKategorie(const vector<Fiszka>& fiszki) {
    set<string> tagSet;
    for (const auto& f : fiszki)
        if (!f.etykieta.empty()) tagSet.insert(f.etykieta);

    if (tagSet.empty()) return ""; // brak tagów — uczymy sie wszystkiego

    czyscEkran();
    cout << "=== WYBIERZ KATEGORIE ===\n\n";

    int wszystkich = (int)fiszki.size();
    cout << "  0. Wszystkie fiszki (" << wszystkich << ")\n";

    vector<string> tagi(tagSet.begin(), tagSet.end());
    for (int i = 0; i < (int)tagi.size(); ++i) {
        int ile = 0;
        for (const auto& f : fiszki) if (f.etykieta == tagi[i]) ile++;
        cout << "  " << (i + 1) << ". " << tagi[i] << " (" << ile << ")\n";
    }

    cout << "\n  Wybor: ";
    int wybor = czytajInt();

    if (wybor >= 1 && wybor <= (int)tagi.size())
        return tagi[wybor - 1];
    return "";
}

// -----------------------------------------------------------------------
// Przegladanie i wyszukiwanie fiszek
// -----------------------------------------------------------------------

static void browseFiszki(const vector<Fiszka>& fiszki) {
    czyscEkran();
    cout << "=== PRZEGLADAJ / SZUKAJ ===\n\n";

    if (fiszki.empty()) {
        cout << "  Talia jest pusta.\n";
        czekajNaEnter();
        return;
    }

    cout << "Szukaj w pytaniach i odpowiedziach (Enter = wszystkie): ";
    string zapytanie;
    getline(cin, zapytanie);

    string zapytanieLower = zapytanie;
    for (char& c : zapytanieLower) c = (char)tolower((unsigned char)c);

    vector<const Fiszka*> wyniki;
    for (const auto& f : fiszki) {
        if (zapytanie.empty()) {
            wyniki.push_back(&f);
        } else {
            string qLow = f.pytanie, aLow = f.odpowiedz;
            for (char& c : qLow) c = (char)tolower((unsigned char)c);
            for (char& c : aLow) c = (char)tolower((unsigned char)c);
            if (qLow.find(zapytanieLower) != string::npos ||
                aLow.find(zapytanieLower) != string::npos)
                wyniki.push_back(&f);
        }
    }

    if (wyniki.empty()) {
        cout << "\n  Brak wynikow dla: \"" << zapytanie << "\"\n";
        czekajNaEnter();
        return;
    }

    auto easeNaProc = [](double e) -> int {
        int p = (int)round((e - 1.3) / (3.5 - 1.3) * 100.0);
        return max(0, min(100, p));
    };

    const int STRONA = 10;
    int strona = 0;
    int liczbaStron = ((int)wyniki.size() + STRONA - 1) / STRONA;

    while (true) {
        czyscEkran();
        cout << "=== WYNIKI";
        if (!zapytanie.empty()) cout << " dla \"" << zapytanie << "\"";
        cout << " (" << wyniki.size() << " fiszek) ===\n\n";

        int pocz = strona * STRONA;
        int kon  = min(pocz + STRONA, (int)wyniki.size());
        for (int i = pocz; i < kon; ++i) {
            const Fiszka* f = wyniki[i];
            cout << "  " << setw(3) << (i + 1) << ". "
                 << "[" << setw(3) << easeNaProc(f->poziomTrudnosci) << "/100]  ";
            string q = f->pytanie;
            if ((int)q.length() > 30) q = q.substr(0, 27) + "...";
            cout << left << setw(32) << q << "-> " << f->odpowiedz;
            if (!f->etykieta.empty()) cout << "  [" << f->etykieta << "]";
            cout << right << "\n";
        }

        cout << "\nStrona " << (strona + 1) << "/" << liczbaStron;
        if (liczbaStron > 1)
            cout << "  [n = nastepna  p = poprzednia  Enter = wyjdz]";
        cout << "\n> ";

        string cmd;
        getline(cin, cmd);
        if (cmd.empty() || cmd == "q") break;
        if ((cmd == "n" || cmd == "N") && strona < liczbaStron - 1) strona++;
        if ((cmd == "p" || cmd == "P") && strona > 0) strona--;
    }
}

// -----------------------------------------------------------------------
// Podmenu: ustawienia (zmiana trybu nauki)
// -----------------------------------------------------------------------

void podmenuUstawien(TrybNauki& trybNauki, int& trybLiterowek) {
    bool dzialaj = true;
    while (dzialaj) {
        czyscEkran();
        cout << "=== USTAWIENIA ===\n\n";
        cout << "Tryb nauki:      "
             << (trybNauki == TrybNauki::RECZNY ? "Samoocena (0-5)" : "Wpisywanie")
             << "\n";
        const char* typoOpis = (trybLiterowek == TRYB_DOKLADNY)    ? "Dokladny (brak tolerancji)" :
                               (trybLiterowek == TRYB_ULGOWY) ? "Ulgowy (do 35% roznic)"     :
                                                              "Normalny (do 20% roznic)";
        cout << "Tolerancja typo: " << typoOpis << "\n\n";
        cout << "--- Tryb nauki ---\n";
        cout << "1. Samoocena   \u2014 sam oceniasz sie w skali 0-5\n";
        cout << "2. Wpisywanie  \u2014 wpisujesz odpowiedz, program sprawdza literowki\n\n";
        cout << "--- Tolerancja literowek ---\n";
        cout << "3. Dokladny    \u2014 tylko idealne odpowiedzi\n";
        cout << "4. Normalny    \u2014 drobne literowki OK (maks. 20%)\n";
        cout << "5. Ulgowy      \u2014 wieksze literowki OK (maks. 35%)\n\n";
        cout << "0. Wr\u00f3c do menu\n\n";
        cout << "Wybor: ";

        int wybor = czytajInt();
        int trybNaukiInt = (trybNauki == TrybNauki::AUTOMATYCZNY) ? 1 : 0;

        switch (wybor) {
            case 1:
                trybNauki = TrybNauki::RECZNY;
                zapiszKonfiguracje(0, trybLiterowek);
                cout << "\n  Tryb zmieniony na: Samoocena\n";
                czekajNaEnter();
                break;
            case 2:
                trybNauki = TrybNauki::AUTOMATYCZNY;
                zapiszKonfiguracje(1, trybLiterowek);
                cout << "\n  Tryb zmieniony na: Wpisywanie\n";
                czekajNaEnter();
                break;
            case 3:
                trybLiterowek = TRYB_DOKLADNY;
                zapiszKonfiguracje(trybNaukiInt, trybLiterowek);
                cout << "\n  Tolerancja zmieniona na: Dokladny\n";
                czekajNaEnter();
                break;
            case 4:
                trybLiterowek = TRYB_NORMALNY;
                zapiszKonfiguracje(trybNaukiInt, trybLiterowek);
                cout << "\n  Tolerancja zmieniona na: Normalny\n";
                czekajNaEnter();
                break;
            case 5:
                trybLiterowek = TRYB_ULGOWY;
                zapiszKonfiguracje(trybNaukiInt, trybLiterowek);
                cout << "\n  Tolerancja zmieniona na: Ulgowy\n";
                czekajNaEnter();
                break;
            case 0:
                dzialaj = false;
                break;
            default:
                cout << "\n  Nieznana opcja. Wpisz 0-5.\n";
                czekajNaEnter();
        }
    }
}

// -----------------------------------------------------------------------
// Statystyki talii
// -----------------------------------------------------------------------

void wyswietlStatystyki(const vector<Fiszka>& fiszki, bool czekaj) {
    czyscEkran();
    cout << "=== STATYSTYKI TALII ===\n\n";

    if (fiszki.empty()) {
        cout << "  Talia jest pusta. Dodaj fiszki przez menu!\n";
        if (czekaj) czekajNaEnter();
        return;
    }

    const Fiszka* najtrudniejsza = &fiszki[0];
    const Fiszka* najwyzszyEase  = &fiszki[0];
    for (const auto& f : fiszki) {
        if (f.poziomTrudnosci < najtrudniejsza->poziomTrudnosci) najtrudniejsza = &f;
        if (f.poziomTrudnosci > najwyzszyEase->poziomTrudnosci)  najwyzszyEase  = &f;
    }

    double sredniEase = 0;
    for (const auto& f : fiszki) sredniEase += f.poziomTrudnosci;
    sredniEase /= fiszki.size();

    auto easeNaProc = [](double e) -> int {
        int p = (int)round((e - 1.3) / (3.5 - 1.3) * 100.0);
        if (p < 0)   p = 0;
        if (p > 100) p = 100;
        return p;
    };

    cout << "  Lacznie fiszek:      " << fiszki.size() << "\n";
    cout << "  Srednia znajomosc:   " << easeNaProc(sredniEase) << "/100\n\n";

    cout << "  Najtrudniejsza:\n";
    cout << "    -> \"" << najtrudniejsza->pytanie << "\""
         << "  [znajomosc: " << easeNaProc(najtrudniejsza->poziomTrudnosci) << "/100]\n\n";

    cout << "  Najlatwiejsza:\n";
    cout << "    -> \"" << najwyzszyEase->pytanie << "\""
         << "  [znajomosc: " << easeNaProc(najwyzszyEase->poziomTrudnosci) << "/100]\n\n";

    // Posortuj i pokaz top-5 najtrudniejszych jako rekomendacje
    vector<const Fiszka*> posortowane;
    posortowane.reserve(fiszki.size());
    for (const auto& f : fiszki) posortowane.push_back(&f);
    sort(posortowane.begin(), posortowane.end(), [](const Fiszka* a, const Fiszka* b) {
        return a->poziomTrudnosci < b->poziomTrudnosci;
    });

    int topN = min(5, (int)posortowane.size());
    cout << "  Zalecane do powtorki (" << topN << " najtrudniejszych):\n";
    for (int i = 0; i < topN; ++i) {
        cout << "    " << (i + 1) << ". \""
             << posortowane[i]->pytanie << "\""
             << "  [" << easeNaProc(posortowane[i]->poziomTrudnosci) << "/100]\n";
    }
    cout << "\n";

    if (czekaj) czekajNaEnter();
}

// -----------------------------------------------------------------------
// GŁÓWNA PĘTLA MENU — wywoływana z main.cpp
// -----------------------------------------------------------------------

void uruchomMenu(vector<Fiszka>& fiszki, TrybNauki& trybNauki, int seria, int& trybLiterowek,
                 int celDzienny, int& fiszkiDzisiaj) {
    bool dziala = true;

    while (dziala) {
        czyscEkran();
        cout << "==========================================\n";
        cout << "               " << APP_NAME << "\n";
        cout << "==========================================\n\n";

        if (seria == 1) {
            cout << "  Dzis zaczynasz serie!\n";
        } else if (seria > 1) {
            cout << "  Jestes na " << seria << "-dniowej serii!\n";
        }

        if (celDzienny > 0) {
            cout << "  Cel dnia: " << fiszkiDzisiaj << "/" << celDzienny << " fiszek";
            if (fiszkiDzisiaj >= celDzienny) cout << "  [OSIAGNIETY!]";
            cout << "\n";
        }

        vector<Fiszka*> doNauki;
        for (auto& f : fiszki) doNauki.push_back(&f);
        if (!doNauki.empty()) {
            cout << "  *** " << doNauki.size()
                 << " fiszek czeka na powtorke! ***\n";
        } else {
            cout << "  Brak fiszek do powtorki.\n";
        }

        cout << "\n  Tryb: "
             << (trybNauki == TrybNauki::RECZNY ? "Samoocena" : "Wpisywanie")
             << "\n\n";

        cout << "  1. Ucz sie\n";
        cout << "  2. Dodaj fiszki\n";
        cout << "  3. Przegladaj / Szukaj\n";
        cout << "  4. Statystyki\n";
        cout << "  5. Ustawienia\n";
        cout << "  0. Wyjdz i zapisz\n\n";
        cout << "  Wybor: ";

        int wybor = czytajInt();

        switch (wybor) {
            case 1: {
                if (doNauki.empty()) {
                    cout << "\n  Brak fiszek do nauki. Dodaj fiszki lub wróc jutro.\n";
                    czekajNaEnter();
                    break;
                }
                string tagFilter = wybierzKategorie(fiszki);
                vector<Fiszka*> sesja;
                if (tagFilter.empty()) {
                    sesja = doNauki;
                } else {
                    for (auto* fp : doNauki)
                        if (fp->etykieta == tagFilter) sesja.push_back(fp);
                    if (sesja.empty()) {
                        cout << "\n  Brak fiszek do nauki w kategorii: " << tagFilter << "\n";
                        czekajNaEnter();
                        break;
                    }
                }
                // Sortuj najtrudniejsze fiszki na poczatek sesji
                sort(sesja.begin(), sesja.end(), [](const Fiszka* a, const Fiszka* b) {
                    return a->poziomTrudnosci < b->poziomTrudnosci;
                });
                float prog = progOdTrybu(trybLiterowek);
                StatystykiSesji stats = (trybNauki == TrybNauki::AUTOMATYCZNY)
                    ? trybWpisywania(sesja, prog)
                    : trybSamooceny(sesja);
                fiszkiDzisiaj += stats.przejrzano;
                zapiszTalie(fiszki);
                if (stats.wynik == WynikSesji::ZAKONCZ) {
                    exit(0);
                }
                break;
            }
            case 2:
                dodajFiszke(fiszki);
                break;
            case 3:
                browseFiszki(fiszki);
                break;
            case 4:
                wyswietlStatystyki(fiszki);
                break;
            case 5:
                podmenuUstawien(trybNauki, trybLiterowek);
                break;
            case 0:
                dziala = false;
                break;
            default:
                cout << "\n  Nieznana opcja. Wpisz 0-5.\n";
                czekajNaEnter();
        }
    }
}