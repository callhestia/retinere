#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <locale>
#include "file_manager.hpp"
#include "../config.hpp"
#include <limits.h>
#include <cstring>
#include <cstdlib>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <unistd.h>
#endif

using namespace std;
// Globalna zmienna przechowująca ścieżkę do pliku z fiszkami.
// Runtime-override deck path; nadpisywany przez flage --deck w main.cpp
string g_sciezkaTalii = PLIK_TALII;

// Funkcja wczytująca wszystkie fiszki z pliku do wektora
vector<Fiszka> wczytajTalie(){
    vector<Fiszka> deck;
    std::locale loc;
    try {
        loc = std::locale("");
    } catch (...) {
        loc = std::locale::classic();
    }

    ifstream file;
    file.imbue(loc); // Przypisanie lokalizacji do strumienia pliku
    file.open(g_sciezkaTalii);
    if (!file.is_open()) {
        // Common fallback candidates (relative to cwd)
        std::vector<std::string> candidates;
        candidates.push_back(std::string("decks/deck.txt"));
        candidates.push_back(std::string("../decks/deck.txt"));
        candidates.push_back(std::string("../../decks/deck.txt"));

        // Try paths relative to the executable directory
        char exePathBuf[PATH_MAX];
        exePathBuf[0] = '\0';
#if defined(__APPLE__)
        uint32_t size = sizeof(exePathBuf);
        if (_NSGetExecutablePath(exePathBuf, &size) == 0) {
            // success
        }
#elif defined(__linux__)
        ssize_t len = readlink("/proc/self/exe", exePathBuf, sizeof(exePathBuf) - 1);
        if (len != -1) exePathBuf[len] = '\0';
#elif defined(_WIN32)
        // Windows: leave exePathBuf empty (we'll rely on cwd)
#endif
        std::string exeDir;
        if (exePathBuf[0] != '\0') {
            std::string exePathStr = exePathBuf;
            auto pos = exePathStr.find_last_of("/\\");
            if (pos != std::string::npos) exeDir = exePathStr.substr(0, pos);
        }

        if (!exeDir.empty()) {
            candidates.push_back(exeDir + "/decks/deck.txt");
            candidates.push_back(exeDir + "/../decks/deck.txt");
        }

        bool found = false;
        for (const auto &cand : candidates) {
            if (file_exists(cand)) {
                pathToOpen = cand;
                file.open(pathToOpen);
                if (file.is_open()) { found = true; break; }
            }
        }

        if (!file.is_open()) {
            // create an empty file at original path so subsequent saves will work
            std::ofstream newFile(g_sciezkaTalii);
            newFile.imbue(loc);
            return deck;
        }
        // if we opened a fallback candidate, update global path so saves go to same file
        if (pathToOpen != g_sciezkaTalii) g_sciezkaTalii = pathToOpen;
    }
    string line;
    while(getline(file, line)){
        if(line.empty()) continue;
        stringstream ss(line);
        string item;
        Fiszka card;
        try{
            getline(ss,card.pytanie, '|');
            getline(ss,card.odpowiedz,'|');
            getline(ss,item,'|'); card.poziomTrudnosci = stod(item);

            // Migracja: stary format mial tu pole combo (liczba calkowita),
            // nowy format ma od razu etykieta. Rozrozniamy po obecnosci '|' w reszcie.
            string rest;
            getline(ss, rest);
            auto sep = rest.find('|');
            if (sep != string::npos) {
                // Stary format: combo|etykieta — pomijamy combo, bierzemy etykieta
                string tagPart = rest.substr(sep + 1);
                if (!tagPart.empty()) card.etykieta = tagPart;
            } else {
                // Nowy format lub stary bez tagu: sprawdz czy to cyfry (stare combo)
                bool sameLiczby = !rest.empty();
                for (char c : rest) if (!isdigit(c)) { sameLiczby = false; break; }
                if (!sameLiczby) card.etykieta = rest;
            }

            deck.push_back(card);
        }
        catch(...){
            continue; // Pominięcie błędnie sformatowanych linii, np. brak pola poziomTrudnosci lub inne problemy z konwersją
        }
    }
    file.close();
    return deck;
}

// Funkcja zapisująca aktualny stan wektora z powrotem do pliku tekstowego
void zapiszTalie(const vector<Fiszka>& deck){
    
    ofstream file;
    file.open(g_sciezkaTalii);
    if(!file.is_open()) return;
    for(const auto& card : deck){
        file<<card.pytanie<<"|"<<card.odpowiedz<<"|"<<card.poziomTrudnosci<<"|"<<card.etykieta<<"\n";
    }
    file.close();
}

// Wczytuje główny tryb nauki
int wczytajKonfiguracje(){
    // Próba ustawienia domyślnej lokalizacji systemu, jeśli się nie uda - program używa klasycznej lokalizacji (C)
    std::locale loc;
    try {
        loc = std::locale("");
    } catch (...) {
        loc = std::locale::classic();
    }

    ifstream file;
    file.imbue(loc);
    file.open(PLIK_KONFIGURACJI);
    int mode = 0;
    if(file.is_open()){
        file>>mode;
    }
    return mode;
}

// Zapisuje wybrane ustawienia (tryb nauki i tryb literówek) do pliku konfiguracyjnego
void zapiszKonfiguracje(int studyMode, int typoMode) {
    std::locale loc;
    try {
        loc = std::locale("");
    } catch (...) {
        loc = std::locale::classic();
    }

    ofstream file;
    file.imbue(loc);
    file.open(PLIK_KONFIGURACJI);
    if(file.is_open()){
        file << studyMode << "\n" << typoMode << "\n";
    }
}
// Wczytuje ustawienie tolerancji na literówki
int wczytajTrybLiterowek() {
    ifstream file(PLIK_KONFIGURACJI);
    if (!file.is_open()) return TRYB_NORMALNY;
    int studyMode = 0, typoMode = TRYB_NORMALNY;
    file >> studyMode >> typoMode;
    if (file.fail()) typoMode = TRYB_NORMALNY; // backward compat: stary config bez linii 2
    return typoMode;
}

// Pobiera dane o passie użytkownika
void wczytajDaneSerii(int& lastDay, int& streak) {
    ifstream file(PLIK_SERII);
    lastDay = 0;
    streak  = 0;
    if (file.is_open()) {
        file >> lastDay >> streak;
    }
}

// Zapisuje aktualną passę dni nauki
void zapiszDaneSerii(int lastDay, int streak) {
    ofstream file(PLIK_SERII);
    if (file.is_open()) {
        file << lastDay << "\n" << streak << "\n";
    }
}

// Zrzuca całą talię do pliku tekstowego z nagłówkiem informacyjnym
bool eksportujTalie(const vector<Fiszka>& deck, const string& path) {
    ofstream file(path);
    if (!file.is_open()) return false;
    file << "# Retinere export — format: pytanie|odpowiedz|poziomTrudnosci|etykieta\n";
    for (const auto& card : deck)
        file << card.pytanie << "|" << card.odpowiedz << "|" << card.poziomTrudnosci << "|" << card.etykieta << "\n";
    return true;
}

// Wczytuje talię z zewnętrznego pliku, zliczając jednocześnie pominięte (błędnie sformatowane) linie
vector<Fiszka> importujTalie(const string& path, int& skipped) {
    vector<Fiszka> result;
    skipped = 0; // Licznik pominiętych linii z powodu błędów formatowania
    ifstream file(path); 
    if (!file.is_open()) { skipped = -1; return result; }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        stringstream ss(line); 
        string q, a, eStr, etykieta;
        if (!getline(ss, q, '|') || q.empty()) { skipped++; continue; }
        if (!getline(ss, a, '|') || a.empty()) { skipped++; continue; }
        Fiszka card;
        card.pytanie = q;
        card.odpowiedz   = a;
        if (getline(ss, eStr, '|')) { 
            try { card.poziomTrudnosci = stod(eStr); } catch (...) { card.poziomTrudnosci = 2.5; } // Domyślny poziom trudności, jeśli konwersja się nie powiedzie
        }
        if (getline(ss, etykieta)) card.etykieta = etykieta;
        result.push_back(card);
    }
    return result;
}

// Wczytuje ile fiszek użytkownik zaplanował zrobić w danym dniu
int wczytajCelDzienny() {
    ifstream file(PLIK_DZIENNY);
    if (!file.is_open()) return DOMYSLNY_CEL_DZIENNY;
    int goal = DOMYSLNY_CEL_DZIENNY;
    file >> goal;
    if (file.fail() || goal <= 0) goal = DOMYSLNY_CEL_DZIENNY;
    return goal;
}

// Sprawdza, ile fiszek użytkownik już powtórzył danego dnia
int wczytajPostepDzienny(int today) {
    ifstream file(PLIK_DZIENNY);
    if (!file.is_open()) return 0;
    int goal = DOMYSLNY_CEL_DZIENNY, lastDay = 0, count = 0;
    file >> goal >> lastDay >> count;
    return (lastDay == today) ? count : 0;
}

// Aktualizuje i zapisuje plik postępów (ile zrobiono dzisiaj)
void zapiszPostepDzienny(int today, int count) {
    int goal = wczytajCelDzienny();
    ofstream file(PLIK_DZIENNY);
    if (file.is_open())
        file << goal << "\n" << today << "\n" << count << "\n";
}
