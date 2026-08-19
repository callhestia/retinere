#ifndef FILE_MANAGER_HPP
#define FILE_MANAGER_HPP

#include <string>
#include <vector>
#include "Flashcard.hpp"

// Runtime-override deck path — ustaw przed pierwszym wczytajTalie/zapiszTalie.
// Domyslnie PLIK_TALII z config.hpp; nadpisywany przez flage --deck.
extern std::string g_sciezkaTalii;

// Display name of the currently loaded deck (from the "name" field in JSON).
extern std::string g_deckName;

// Info about one deck file found in the decks/ directory.
struct DeckInfo {
    std::string path;  // full/relative path to the .json file
    std::string name;  // human-readable name from the "name" field
};

std::vector<Fiszka> wczytajTalie();
void zapiszTalie(const std::vector<Fiszka>& deck);

// Scan the decks/ directory and return info for every .json file found.
std::vector<DeckInfo> listujTalie();

// Create a new empty deck file. Returns true on success.
bool stworzTalie(const std::string& filename, const std::string& name);

// studyMode: 0=RECZNY, 1=AUTOMATYCZNY  |  typoMode: 0=strict, 1=normal, 2=forgiving  |  language: "pl"|"en"
void zapiszKonfiguracje(int studyMode, int typoMode = 1, const std::string& language = "pl");
int         wczytajKonfiguracje();    // zwraca studyMode
int         wczytajTrybLiterowek();  // zwraca typoMode (domyslnie 1 gdy brak drugiej linii)
std::string wczytajJezyk();          // zwraca kod jezyka (domyslnie "pl" gdy brak linii 3)

// Streak: lastDay = dni od epoki Unix; streak = licznik dni z rzedu
void wczytajDaneSerii(int& lastDay, int& streak);
void zapiszDaneSerii(int lastDay, int streak);

// Eksport/import talii w formacie pipe-separated (pytanie|odpowiedz|poziomTrudnosci|etykieta)
// eksportujTalie: zwraca true gdy plik zapisany pomyslnie
bool eksportujTalie(const std::vector<Fiszka>& deck, const std::string& path);
// importujTalie: skipped=-1 gdy plik nieznaleziony; >=0 liczba blednych linii
std::vector<Fiszka> importujTalie(const std::string& path, int& skipped);

// Dzienny cel i postep (daily.txt: linia1=goal  linia2=lastDay  linia3=count)
// wczytajPostepDzienny: zwraca 0 gdy nowy dzien (today != lastDay zapisany)
int  wczytajCelDzienny();
int  wczytajPostepDzienny(int today);
void zapiszPostepDzienny(int today, int count);

#endif
