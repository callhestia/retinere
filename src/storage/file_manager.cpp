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

// Runtime-override deck path; nadpisywany przez flage --deck w main.cpp
string g_sciezkaTalii = PLIK_TALII;

vector<Fiszka> wczytajTalie(){
    vector<Fiszka> deck;
    std::locale loc;
    try {
        loc = std::locale("");
    } catch (...) {
        loc = std::locale::classic();
    }

    auto file_exists = [&](const std::string &p)->bool { std::ifstream t(p); return t.good(); };

    // Try to open the declared path; if it doesn't exist, try common alternatives
    std::string pathToOpen = g_sciezkaTalii;
    std::ifstream file;
    file.imbue(loc);
    file.open(pathToOpen);

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
            continue;
        }
    }
    file.close();
    return deck;
}

void zapiszTalie(const vector<Fiszka>& deck){
    std::locale loc;
    try {
        loc = std::locale("");
    } catch (...) {
        loc = std::locale::classic();
    }

    ofstream file;
    file.imbue(loc);
    file.open(g_sciezkaTalii);
    if(!file.is_open()) return;
    for(const auto& card : deck){
        file<<card.pytanie<<"|"<<card.odpowiedz<<"|"<<card.poziomTrudnosci<<"|"<<card.etykieta<<"\n";
    }
    file.close();
}

int wczytajKonfiguracje(){
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

int wczytajTrybLiterowek() {
    ifstream file(PLIK_KONFIGURACJI);
    if (!file.is_open()) return TRYB_NORMALNY;
    int studyMode = 0, typoMode = TRYB_NORMALNY;
    file >> studyMode >> typoMode;
    if (file.fail()) typoMode = TRYB_NORMALNY; // backward compat: stary config bez linii 2
    return typoMode;
}

void wczytajDaneSerii(int& lastDay, int& streak) {
    ifstream file(PLIK_SERII);
    lastDay = 0;
    streak  = 0;
    if (file.is_open()) {
        file >> lastDay >> streak;
    }
}

void zapiszDaneSerii(int lastDay, int streak) {
    ofstream file(PLIK_SERII);
    if (file.is_open()) {
        file << lastDay << "\n" << streak << "\n";
    }
}

bool eksportujTalie(const vector<Fiszka>& deck, const string& path) {
    ofstream file(path);
    if (!file.is_open()) return false;
    file << "# Retinere export — format: pytanie|odpowiedz|poziomTrudnosci|etykieta\n";
    for (const auto& card : deck)
        file << card.pytanie << "|" << card.odpowiedz << "|" << card.poziomTrudnosci << "|" << card.etykieta << "\n";
    return true;
}

vector<Fiszka> importujTalie(const string& path, int& skipped) {
    vector<Fiszka> result;
    skipped = 0;
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
            try { card.poziomTrudnosci = stod(eStr); } catch (...) { card.poziomTrudnosci = 2.5; }
        }
        if (getline(ss, etykieta)) card.etykieta = etykieta;
        result.push_back(card);
    }
    return result;
}

int wczytajCelDzienny() {
    ifstream file(PLIK_DZIENNY);
    if (!file.is_open()) return DOMYSLNY_CEL_DZIENNY;
    int goal = DOMYSLNY_CEL_DZIENNY;
    file >> goal;
    if (file.fail() || goal <= 0) goal = DOMYSLNY_CEL_DZIENNY;
    return goal;
}

int wczytajPostepDzienny(int today) {
    ifstream file(PLIK_DZIENNY);
    if (!file.is_open()) return 0;
    int goal = DOMYSLNY_CEL_DZIENNY, lastDay = 0, count = 0;
    file >> goal >> lastDay >> count;
    return (lastDay == today) ? count : 0;
}

void zapiszPostepDzienny(int today, int count) {
    int goal = wczytajCelDzienny();
    ofstream file(PLIK_DZIENNY);
    if (file.is_open())
        file << goal << "\n" << today << "\n" << count << "\n";
}
