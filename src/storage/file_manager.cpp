#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <locale>
#include <filesystem>
#include "file_manager.hpp"
#include "../config.hpp"
#include <cstring>
#include <cstdlib>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#endif

using namespace std;
// Globalna zmienna przechowująca ścieżkę do pliku z fiszkami.
// Runtime-override deck path; nadpisywany przez flage --deck w main.cpp
string g_sciezkaTalii = PLIK_TALII;

// Human-readable name of the currently loaded deck.
string g_deckName = "";

// ── JSON helpers ─────────────────────────────────────────────────────────

static string jsonEsc(const string& s) {
    string r;
    for (unsigned char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20)  {}
        else                r += (char)c;
    }
    return r;
}

// Parse a JSON string value; pos must be right after the opening '"'.
// Advances pos past the closing '"'.
static string jsonParseStr(const string& src, size_t& pos) {
    string r;
    while (pos < src.size()) {
        char c = src[pos++];
        if (c == '"') return r;
        if (c == '\\' && pos < src.size()) {
            char e = src[pos++];
            switch (e) {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case '/':  r += '/';  break;
                case 'n':  r += '\n'; break;
                case 'r':  r += '\r'; break;
                case 't':  r += '\t'; break;
                default:   r += e;    break;
            }
        } else {
            r += c;
        }
    }
    return r;
}

// Parse a complete JSON array of card objects: [{...}, ...]
// pos must point at the opening '['.
static vector<Fiszka> parseCardsArray(const string& src, size_t& pos) {
    vector<Fiszka> deck;
    auto ws = [&]() {
        while (pos < src.size() && (unsigned char)src[pos] <= ' ') ++pos;
    };
    if (pos >= src.size() || src[pos] != '[') return deck;
    ++pos;
    while (pos < src.size()) {
        ws();
        if (pos >= src.size() || src[pos] == ']') { ++pos; break; }
        if (src[pos] == ',') { ++pos; continue; }
        if (src[pos] != '{') { ++pos; continue; }
        ++pos;
        Fiszka card;
        bool hasQ = false;
        while (pos < src.size()) {
            ws();
            if (pos >= src.size()) break;
            if (src[pos] == '}') { ++pos; break; }
            if (src[pos] == ',') { ++pos; continue; }
            if (src[pos] != '"') { ++pos; continue; }
            ++pos;
            string key = jsonParseStr(src, pos);
            ws();
            if (pos < src.size() && src[pos] == ':') ++pos;
            ws();
            if (pos >= src.size()) break;
            if (src[pos] == '"') {
                ++pos;
                string val = jsonParseStr(src, pos);
                if      (key == "pytanie")   { card.pytanie   = val; hasQ = true; }
                else if (key == "odpowiedz") { card.odpowiedz = val; }
                else if (key == "etykieta")  { card.etykieta  = val; }
            } else {
                size_t end = src.find_first_of(",}\n", pos);
                if (end == string::npos) end = src.size();
                string num = src.substr(pos, end - pos);
                while (!num.empty() && (unsigned char)num.back() <= ' ') num.pop_back();
                pos = end;
                if (key == "poziomTrudnosci" && !num.empty())
                    try { card.poziomTrudnosci = stod(num); } catch (...) {}
            }
        }
        if (hasQ) deck.push_back(card);
    }
    return deck;
}

// Parse a complete JSON deck — handles two formats:
//   New wrapped:  {"name": "...", "cards": [...]}
//   Legacy plain: [...]
static vector<Fiszka> parseJsonDeck(const string& src) {
    vector<Fiszka> deck;
    size_t pos = 0;
    auto ws = [&]() {
        while (pos < src.size() && (unsigned char)src[pos] <= ' ') ++pos;
    };
    ws();
    if (pos >= src.size()) return deck;

    if (src[pos] == '[') {
        // Legacy plain array — keep g_deckName as-is
        return parseCardsArray(src, pos);
    }

    if (src[pos] == '{') {
        // New wrapped format: {"name": "...", "cards": [...]}
        ++pos;
        while (pos < src.size()) {
            ws();
            if (pos >= src.size() || src[pos] == '}') break;
            if (src[pos] == ',') { ++pos; continue; }
            if (src[pos] != '"') { ++pos; continue; }
            ++pos;
            string key = jsonParseStr(src, pos);
            ws();
            if (pos < src.size() && src[pos] == ':') ++pos;
            ws();
            if (pos >= src.size()) break;
            if (key == "name" && src[pos] == '"') {
                ++pos;
                g_deckName = jsonParseStr(src, pos);
            } else if (key == "cards" && src[pos] == '[') {
                deck = parseCardsArray(src, pos);
            } else {
                // Skip unknown value
                int depth = 0;
                while (pos < src.size()) {
                    char c = src[pos++];
                    if (c == '{' || c == '[') ++depth;
                    else if (c == '}' || c == ']') { if (--depth < 0) { --pos; break; } }
                    else if (c == '"') { jsonParseStr(src, pos); }
                    else if ((c == ',' || c == '}') && depth == 0) { --pos; break; }
                }
            }
        }
        return deck;
    }

    return deck;
}


// Backward-compat: parse old pipe-separated TXT format
static vector<Fiszka> parseTxtDeck(ifstream& file) {
    vector<Fiszka> deck;
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string item;
        Fiszka card;
        try {
            getline(ss, card.pytanie,   '|');
            getline(ss, card.odpowiedz, '|');
            getline(ss, item,           '|');
            card.poziomTrudnosci = stod(item);
            string rest;
            getline(ss, rest);
            auto sep = rest.find('|');
            if (sep != string::npos) {
                string tag = rest.substr(sep + 1);
                if (!tag.empty()) card.etykieta = tag;
            } else {
                bool onlyDigits = !rest.empty();
                for (char c : rest) if (!isdigit(c)) { onlyDigits = false; break; }
                if (!onlyDigits) card.etykieta = rest;
            }
            deck.push_back(card);
        } catch (...) { continue; }
    }
    return deck;
}

// Resolve the executable directory (same logic as gui_main.cpp)
static string getExeDirFM() {
    char buf[PATH_MAX];
    buf[0] = '\0';
#if defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) buf[len] = '\0';
#elif defined(__APPLE__)
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) != 0) buf[0] = '\0';
#elif defined(_WIN32)
    GetModuleFileNameA(NULL, buf, PATH_MAX);
#endif
    if (buf[0] == '\0') return "";
    string p(buf);
    auto pos = p.find_last_of("/\\");
    return (pos != string::npos) ? p.substr(0, pos) : "";
}

// Funkcja wczytujaca fiszki z pliku JSON (z automatyczna migracja z TXT)
vector<Fiszka> wczytajTalie() {
    vector<Fiszka> deck;
    const string exeDir = getExeDirFM();

    // ── Try JSON candidates ───────────────────────────────────────────────
    vector<string> jsonCandidates = {
        g_sciezkaTalii,
        "decks/deck.json",
        "../decks/deck.json",
        "../../decks/deck.json",
    };
    if (!exeDir.empty()) {
        jsonCandidates.push_back(exeDir + "/decks/deck.json");
        jsonCandidates.push_back(exeDir + "/../decks/deck.json");
    }
    for (const auto& path : jsonCandidates) {
        ifstream f(path);
        if (!f.is_open()) continue;
        stringstream ss;
        ss << f.rdbuf();
        deck = parseJsonDeck(ss.str());
        if (path != g_sciezkaTalii) g_sciezkaTalii = path;
        return deck;
    }

    // ── Migration: try old TXT candidates ────────────────────────────────
    vector<string> txtCandidates = {
        "decks/deck.txt",
        "../decks/deck.txt",
        "../../decks/deck.txt",
    };
    if (!exeDir.empty()) {
        txtCandidates.push_back(exeDir + "/decks/deck.txt");
        txtCandidates.push_back(exeDir + "/../decks/deck.txt");
    }
    for (const auto& path : txtCandidates) {
        ifstream f(path);
        if (!f.is_open()) continue;
        deck = parseTxtDeck(f);
        // Derive JSON path by replacing .txt extension
        string jsonPath = path;
        auto dot = jsonPath.rfind(".txt");
        if (dot != string::npos) jsonPath.replace(dot, 4, ".json");
        g_sciezkaTalii = jsonPath;
        // Auto-save in new format
        zapiszTalie(deck);
        return deck;
    }

    // ── Nothing found: ensure directory exists for future saves ──────────
    try {
        auto dir = filesystem::path(g_sciezkaTalii).parent_path();
        if (!dir.empty()) filesystem::create_directories(dir);
    } catch (...) {}
    return deck;
}

// Zapisuje talie w formacie JSON: {"name":"...","cards":[...]}
void zapiszTalie(const vector<Fiszka>& deck) {
    try {
        auto dir = filesystem::path(g_sciezkaTalii).parent_path();
        if (!dir.empty()) filesystem::create_directories(dir);
    } catch (...) {}

    ofstream file(g_sciezkaTalii);
    if (!file.is_open()) return;

    file << "{\n"
         << "  \"name\": \"" << jsonEsc(g_deckName) << "\",\n"
         << "  \"cards\": [\n";
    for (size_t i = 0; i < deck.size(); ++i) {
        const auto& c = deck[i];
        file << "    {\n"
             << "      \"pytanie\": \""       << jsonEsc(c.pytanie)        << "\",\n"
             << "      \"odpowiedz\": \""     << jsonEsc(c.odpowiedz)      << "\",\n"
             << "      \"poziomTrudnosci\": "  << c.poziomTrudnosci         << ",\n"
             << "      \"etykieta\": \""      << jsonEsc(c.etykieta)       << "\"\n"
             << "    }";
        if (i + 1 < deck.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n}\n";
}

// Resolves the best existing decks/ directory, trying several candidates.
static string findDecksDir() {
    const string exeDir = getExeDirFM();
    vector<string> candidates = { "decks", "../decks", "../../decks" };
    if (!exeDir.empty()) {
        candidates.push_back(exeDir + "/decks");
        candidates.push_back(exeDir + "/../decks");
    }
    for (const auto& d : candidates)
        if (filesystem::exists(d) && filesystem::is_directory(d)) return d;
    // Fall back to the directory of the current deck file
    auto parent = filesystem::path(g_sciezkaTalii).parent_path().string();
    return parent.empty() ? "decks" : parent;
}

// Skanuje katalog decks/ i zwraca liste dostepnych plikow JSON.
vector<DeckInfo> listujTalie() {
    vector<DeckInfo> result;
    string dir = findDecksDir();
    try {
        for (const auto& entry : filesystem::directory_iterator(dir)) {
            if (entry.path().extension() != ".json") continue;
            string path = entry.path().string();
            ifstream f(path);
            if (!f.is_open()) continue;
            stringstream ss; ss << f.rdbuf();
            string content = ss.str();
            // Try to read "name" field
            string name;
            size_t p = content.find("\"name\"");
            if (p != string::npos) {
                p = content.find(':', p);
                if (p != string::npos) {
                    ++p;
                    while (p < content.size() && (unsigned char)content[p] <= ' ') ++p;
                    if (p < content.size() && content[p] == '"') {
                        ++p;
                        name = jsonParseStr(content, p);
                    }
                }
            }
            if (name.empty()) name = entry.path().stem().string();
            result.push_back({path, name});
        }
    } catch (...) {}
    return result;
}

// Tworzy nowy pusty plik talii.
bool stworzTalie(const string& filename, const string& name) {
    string dir = findDecksDir();
    try { filesystem::create_directories(dir); } catch (...) {}
    string path = dir + "/" + filename;
    if (filesystem::exists(path)) return false; // nie nadpisuj
    ofstream f(path);
    if (!f.is_open()) return false;
    f << "{\n  \"name\": \"" << jsonEsc(name) << "\",\n  \"cards\": []\n}\n";
    return true;
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

// Zapisuje wybrane ustawienia (tryb nauki, tryb literówek, język) do pliku konfiguracyjnego
void zapiszKonfiguracje(int studyMode, int typoMode, const std::string& language) {
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
        file << studyMode << "\n" << typoMode << "\n" << language << "\n";
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
// Wczytuje kod języka interfejsu (linia 3 config.txt)
std::string wczytajJezyk() {
    ifstream file(PLIK_KONFIGURACJI);
    if (!file.is_open()) return "pl";
    int studyMode = 0, typoMode = 0;
    std::string lang;
    file >> studyMode >> typoMode >> lang;
    if (file.fail() || lang.empty()) return "pl";
    return lang;
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
