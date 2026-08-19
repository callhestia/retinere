#define WEBVIEW_IMPLEMENTATION
#include "webview.h"
#include "src/storage/file_manager.hpp"
#include "src/engine/engine.hpp"
#include "src/config.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <filesystem>

std::vector<Fiszka> g_fiszki;
int g_poprzedniIndeks = -1;
TrybNauki g_trybNauki = TrybNauki::AUTOMATYCZNY;
int g_trybLiterowek  = TRYB_NORMALNY;
std::string g_jezyk  = "pl";

// Session state – managed by inicjujSesjeCpm / pobierzFiszkeCpm
static std::vector<int> g_sesjaKolejka;   // cards still to show this session
static std::set<int>    g_sesjaMastered;  // card indices that have passed
static int              g_sesjaTotal = 0; // unique cards in the current session
static int              g_sesjaRatings[6] = {0,0,0,0,0,0}; // answer counts per rating 1-5
static bool             g_sesjaIntensive = false; // true = intensive exam mode

// Escape string for embedding in JSON
static std::string je(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
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

static int easeNaProcent(double e) {
    int p = (int)std::round((e - 1.3) / (3.5 - 1.3) * 100.0);
    return std::max(0, std::min(100, p));
}

std::string wyciagnijZJson(const std::string& json, const std::string& klucz) {
    size_t pos = json.find("\"" + klucz + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    size_t valStart = json.find_first_not_of(" \t\n\r", pos + 1);
    if (valStart == std::string::npos) return "";
    if (json[valStart] == '"') {
        size_t start = valStart + 1;
        size_t end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    } else {
        size_t end = json.find_first_of(",}]", valStart);
        if (end == std::string::npos) end = json.size();
        return json.substr(valStart, end - valStart);
    }
}

static std::string getExeDir() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1) return ".";
    buf[len] = '\0';
    std::string path(buf);
    return path.substr(0, path.rfind('/'));
}

static std::string g_buildDir;

static std::string mimeForPath(const std::string& p) {
    auto ends = [&](const char* s){ return p.size() > strlen(s) && p.substr(p.size()-strlen(s)) == s; };
    if (ends(".js"))    return "application/javascript";
    if (ends(".css"))   return "text/css";
    if (ends(".html"))  return "text/html";
    if (ends(".svg"))   return "image/svg+xml";
    if (ends(".png"))   return "image/png";
    if (ends(".ico"))   return "image/x-icon";
    if (ends(".woff2")) return "font/woff2";
    return "application/octet-stream";
}

static void appSchemeHandler(WebKitURISchemeRequest* req, gpointer) {
    std::string uri(webkit_uri_scheme_request_get_uri(req));
    std::string path;
    const std::string host = "app://localhost";
    if (uri.rfind(host, 0) == 0) path = uri.substr(host.size());
    if (path.empty() || path == "/") path = "/index.html";
    auto q = path.find('?'); if (q != std::string::npos) path = path.substr(0, q);
    auto f = path.find('#'); if (f != std::string::npos) path = path.substr(0, f);

    std::string filePath = g_buildDir + path;
    gchar* data = nullptr; gsize len = 0; GError* err = nullptr;
    if (!g_file_get_contents(filePath.c_str(), &data, &len, &err)) {
        g_clear_error(&err);
        filePath = g_buildDir + "/index.html";
        if (!g_file_get_contents(filePath.c_str(), &data, &len, &err)) {
            webkit_uri_scheme_request_finish_error(req, err);
            g_error_free(err); return;
        }
    }
    GInputStream* stream = g_memory_input_stream_new_from_data(data, (gssize)len, g_free);
    webkit_uri_scheme_request_finish(req, stream, (gint64)len, mimeForPath(filePath).c_str());
    g_object_unref(stream);
}

int main() {
    g_fiszki       = wczytajTalie();
    int sm         = wczytajKonfiguracje();
    g_trybNauki    = (sm == 1) ? TrybNauki::AUTOMATYCZNY : TrybNauki::RECZNY;
    g_trybLiterowek = wczytajTrybLiterowek();
    g_jezyk         = wczytajJezyk();
    g_buildDir     = getExeDir() + "/../../build";

    webkit_web_context_register_uri_scheme(
        webkit_web_context_get_default(), "app", appSchemeHandler, nullptr, nullptr);

    webview::webview w(true, nullptr);
    w.set_title("Zora");
    w.set_size(920, 800, WEBVIEW_HINT_NONE);

    // Set window icon from Zora.png
    {
        std::string iconPath = getExeDir() + "/Zora.png";
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size(iconPath.c_str(), 256, 256, nullptr);
        if (pixbuf) {
            gtk_window_set_icon(GTK_WINDOW(w.window()), pixbuf);
            g_object_unref(pixbuf);
        }
    }

    // ── inicjujSesjeCpm() — initialise a review session ───────────────────
    w.bind("inicjujSesjeCpm", [](const std::string& req) -> std::string {
        g_sesjaKolejka.clear();
        g_sesjaMastered.clear();
        // Parse intensive flag from payload
        std::string intensiveStr = wyciagnijZJson(req, "intensive");
        g_sesjaIntensive = (intensiveStr == "true" || intensiveStr == "1");
        if (g_fiszki.empty()) {
            g_sesjaTotal = 0;
            for (int i = 0; i < 6; i++) g_sesjaRatings[i] = 0;
            return "{\"total\":0}";
        }
        if (g_sesjaIntensive) {
            // Intensive mode: queue ALL cards in the deck, shuffled
            std::vector<int> indices;
            indices.reserve(g_fiszki.size());
            for (int i = 0; i < (int)g_fiszki.size(); i++) indices.push_back(i);
            for (int i = (int)indices.size() - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                std::swap(indices[i], indices[j]);
            }
            for (int idx : indices) g_sesjaKolejka.push_back(idx);
            g_sesjaTotal = (int)g_fiszki.size();
        } else {
            // Daily mode: top 20 hardest cards sorted by ease factor ascending
            std::vector<std::pair<float, int>> sorted;
            for (int i = 0; i < (int)g_fiszki.size(); i++)
                sorted.push_back({g_fiszki[i].poziomTrudnosci, i});
            std::sort(sorted.begin(), sorted.end());
            int n = std::min((int)sorted.size(), 20);
            for (int i = 0; i < n; i++)
                g_sesjaKolejka.push_back(sorted[i].second);
            g_sesjaTotal = n;
        }
        for (int i = 0; i < 6; i++) g_sesjaRatings[i] = 0;
        g_poprzedniIndeks = -1;
        std::stringstream ss;
        ss << "{\"total\":" << g_sesjaTotal << "}";
        return ss.str();
    });

    // ── pobierzFiszkeCpm() ────────────────────────────────────────────────
    w.bind("pobierzFiszkeCpm", [](const std::string&) -> std::string {
        if (g_fiszki.empty())
            return "{\"id\":-1,\"question\":\"Brak fiszek w talii\",\"label\":\"\"}";
        // Active session: serve from queue
        if (g_sesjaTotal > 0) {
            if (g_sesjaKolejka.empty()) {
                // All cards mastered – session complete
                int tA = 0, wS = 0;
                for (int i = 1; i <= 5; i++) { tA += g_sesjaRatings[i]; wS += i * g_sesjaRatings[i]; }
                int scorePct = tA > 0 ? (int)std::round((double)wS / (5.0 * tA) * 100.0) : 0;
                std::stringstream ss;
                ss << "{\"id\":-1,\"done\":true,\"mastered\":" << (int)g_sesjaMastered.size()
                   << ",\"total\":" << g_sesjaTotal
                   << ",\"score\":" << scorePct
                   << ",\"r1\":" << g_sesjaRatings[1]
                   << ",\"r2\":" << g_sesjaRatings[2]
                   << ",\"r3\":" << g_sesjaRatings[3]
                   << ",\"r4\":" << g_sesjaRatings[4]
                   << ",\"r5\":" << g_sesjaRatings[5] << "}";
                return ss.str();
            }
            int idx = g_sesjaKolejka.front();
            g_sesjaKolejka.erase(g_sesjaKolejka.begin());
            g_poprzedniIndeks = idx;
            std::stringstream ss;
            ss << "{\"id\":" << idx
               << ",\"question\":\"" << je(g_fiszki[idx].pytanie) << "\""
               << ",\"answer\":\""  << je(g_fiszki[idx].odpowiedz) << "\""
               << ",\"label\":\""   << je(g_fiszki[idx].etykieta) << "\""
               << ",\"mastered\":"  << (int)g_sesjaMastered.size()
               << ",\"total\":"     << g_sesjaTotal << "}";
            return ss.str();
        }
        // No active session – legacy weighted random (fallback)
        std::vector<double> wagi;
        for (const auto& f : g_fiszki) wagi.push_back(4.8 - f.poziomTrudnosci);
        int idx = wybierzNastepna(wagi, g_poprzedniIndeks);
        if (idx < 0) idx = 0;
        g_poprzedniIndeks = idx;
        std::stringstream ss;
        ss << "{\"id\":" << idx
           << ",\"question\":\"" << je(g_fiszki[idx].pytanie) << "\""
           << ",\"answer\":\"" << je(g_fiszki[idx].odpowiedz) << "\""
           << ",\"label\":\"" << je(g_fiszki[idx].etykieta) << "\"}";
        return ss.str();
    });

    // ── sprawdzOdpowiedzCpm({id, answer}) ─────────────────────────────────
    w.bind("sprawdzOdpowiedzCpm", [](const std::string& req) -> std::string {
        std::string idStr = wyciagnijZJson(req, "id");
        std::string odp   = wyciagnijZJson(req, "answer");
        if (idStr.empty() || g_fiszki.empty())
            return "{\"status\":2,\"correct\":\"\",\"new_interval\":0,\"mastered\":0,\"total\":0}";
        int id = std::stoi(idStr);
        if (id < 0 || id >= (int)g_fiszki.size())
            return "{\"status\":2,\"correct\":\"\",\"new_interval\":0,\"mastered\":0,\"total\":0}";
        Fiszka& fiszka = g_fiszki[id];
        float prog = progOdTrybu(g_trybLiterowek);
        int status = obliczStatusOdpowiedzi(odp, fiszka, prog);
        // Only update SM-2 in daily (Maraton) mode; Sprint mode preserves the schedule
        if (!g_sesjaIntensive) {
            g_fiszki[id] = krokNauki(g_fiszki[id], TrybNauki::AUTOMATYCZNY, odp);
            zapiszTalie(g_fiszki);
        }
        // Map answer status to session rating (for end-of-session summary)
        if (status == 0) g_sesjaRatings[5]++;
        else if (status == 1) g_sesjaRatings[4]++;
        else g_sesjaRatings[1]++;
        // Session tracking – re-queue failed cards
        if (g_sesjaTotal > 0) {
            if (status <= 1) {
                g_sesjaMastered.insert(id);
            } else {
                g_sesjaMastered.erase(id);
                g_sesjaKolejka.push_back(id);
            }
        }
        std::stringstream ss;
        ss << "{\"status\":" << status
           << ",\"correct\":\"" << je(fiszka.odpowiedz) << "\""
           << ",\"new_interval\":" << fiszka.poziomTrudnosci
           << ",\"mastered\":" << (int)g_sesjaMastered.size()
           << ",\"total\":"    << g_sesjaTotal << "}";
        return ss.str();
    });

    // ── ocenFiszkeCpm({id, rating})  — manual mode 1-5 rating ────────────
    w.bind("ocenFiszkeCpm", [](const std::string& req) -> std::string {
        std::string idStr  = wyciagnijZJson(req, "id");
        std::string ratStr = wyciagnijZJson(req, "rating");
        if (idStr.empty() || ratStr.empty() || g_fiszki.empty())
            return "{\"ok\":false,\"mastered\":0,\"total\":0}";
        int id = std::stoi(idStr);
        int rating = std::stoi(ratStr);
        if (id < 0 || id >= (int)g_fiszki.size())
            return "{\"ok\":false,\"mastered\":0,\"total\":0}";
        // Only update SM-2 intervals in daily mode; intensive mode preserves the schedule
        if (!g_sesjaIntensive) {
            g_fiszki[id] = krokNauki(g_fiszki[id], TrybNauki::RECZNY, "", rating);
            zapiszTalie(g_fiszki);
        }
        if (rating >= 1 && rating <= 5) g_sesjaRatings[rating]++;
        // Mastery threshold: 4+ for daily (strict), 3+ for intensive (lenient cramming)
        int masteryThreshold = g_sesjaIntensive ? 3 : 4;
        if (g_sesjaTotal > 0) {
            if (rating >= masteryThreshold) {
                g_sesjaMastered.insert(id);
            } else {
                g_sesjaMastered.erase(id);
                g_sesjaKolejka.push_back(id);
            }
        }
        std::stringstream ss;
        ss << "{\"ok\":true,\"mastered\":" << (int)g_sesjaMastered.size()
           << ",\"total\":" << g_sesjaTotal << "}";
        return ss.str();
    });

    // ── pobierzUstawieniaCpm() ────────────────────────────────────────────
    w.bind("pobierzUstawieniaCpm", [](const std::string&) -> std::string {
        int sm = (g_trybNauki == TrybNauki::AUTOMATYCZNY) ? 1 : 0;
        std::stringstream ss;
        ss << "{\"studyMode\":" << sm
           << ",\"typoMode\":" << g_trybLiterowek
           << ",\"language\":\"" << je(g_jezyk) << "\"}";
        return ss.str();
    });

    // ── zapiszUstawieniaCpm({studyMode, typoMode, language}) ─────────────
    w.bind("zapiszUstawieniaCpm", [](const std::string& req) -> std::string {
        std::string smStr = wyciagnijZJson(req, "studyMode");
        std::string tmStr = wyciagnijZJson(req, "typoMode");
        std::string lang  = wyciagnijZJson(req, "language");
        if (!smStr.empty()) {
            int sm = std::stoi(smStr);
            g_trybNauki = (sm == 1) ? TrybNauki::AUTOMATYCZNY : TrybNauki::RECZNY;
        }
        if (!tmStr.empty()) g_trybLiterowek = std::stoi(tmStr);
        if (!lang.empty())  g_jezyk = lang;
        zapiszKonfiguracje((g_trybNauki == TrybNauki::AUTOMATYCZNY) ? 1 : 0, g_trybLiterowek, g_jezyk);
        return "{\"ok\":true}";
    });

    // ── pobierzInfoTaliiCpm() ─────────────────────────────────────────────
    w.bind("pobierzInfoTaliiCpm", [](const std::string&) -> std::string {
        std::map<std::string, int> cats;
        for (const auto& f : g_fiszki)
            if (!f.etykieta.empty()) cats[f.etykieta]++;
        std::stringstream ss;
        ss << "{\"path\":\"" << je(g_sciezkaTalii) << "\""
           << ",\"name\":\"" << je(g_deckName) << "\""
           << ",\"total\":" << g_fiszki.size()
           << ",\"categories\":[";
        bool first = true;
        for (const auto& kv : cats) {
            if (!first) ss << ",";
            ss << "{\"name\":\"" << je(kv.first) << "\",\"count\":" << kv.second << "}";
            first = false;
        }
        ss << "]}";
        return ss.str();
    });

    // ── dodajFiszkeCpm({question, answer, label}) ─────────────────────────
    w.bind("dodajFiszkeCpm", [](const std::string& req) -> std::string {
        std::string q = wyciagnijZJson(req, "question");
        std::string a = wyciagnijZJson(req, "answer");
        std::string l = wyciagnijZJson(req, "label");
        if (q.empty() || a.empty() || l.empty())
            return "{\"ok\":false,\"total\":0,\"error\":\"Pytanie, odpowiedz i kategoria sa wymagane\"}";
        Fiszka f;
        f.pytanie = q; f.odpowiedz = a; f.etykieta = l; f.poziomTrudnosci = 2.5;
        g_fiszki.push_back(f);
        zapiszTalie(g_fiszki);
        g_poprzedniIndeks = -1;
        std::stringstream ss;
        ss << "{\"ok\":true,\"total\":" << g_fiszki.size() << "}";
        return ss.str();
    });

    // ── pobierzStatystykiCpm() ────────────────────────────────────────────
    w.bind("pobierzStatystykiCpm", [](const std::string&) -> std::string {
        if (g_fiszki.empty())
            return "{\"total\":0,\"avg_pct\":0,\"hardest\":{\"q\":\"\",\"pct\":0},"
                   "\"easiest\":{\"q\":\"\",\"pct\":0},\"top5\":[],\"categories\":[]}";
        double sum = 0;
        const Fiszka* hardest = &g_fiszki[0];
        const Fiszka* easiest = &g_fiszki[0];
        for (const auto& f : g_fiszki) {
            sum += f.poziomTrudnosci;
            if (f.poziomTrudnosci < hardest->poziomTrudnosci) hardest = &f;
            if (f.poziomTrudnosci > easiest->poziomTrudnosci) easiest = &f;
        }
        double avg = sum / g_fiszki.size();
        std::vector<const Fiszka*> sorted;
        for (const auto& f : g_fiszki) sorted.push_back(&f);
        std::sort(sorted.begin(), sorted.end(), [](const Fiszka* a, const Fiszka* b){
            return a->poziomTrudnosci < b->poziomTrudnosci;
        });
        std::map<std::string, int> cats;
        for (const auto& f : g_fiszki)
            if (!f.etykieta.empty()) cats[f.etykieta]++;
        std::stringstream ss;
        ss << "{\"total\":" << g_fiszki.size()
           << ",\"avg_pct\":" << easeNaProcent(avg)
           << ",\"hardest\":{\"q\":\"" << je(hardest->pytanie) << "\",\"pct\":" << easeNaProcent(hardest->poziomTrudnosci) << "}"
           << ",\"easiest\":{\"q\":\"" << je(easiest->pytanie) << "\",\"pct\":" << easeNaProcent(easiest->poziomTrudnosci) << "}"
           << ",\"top5\":[";
        int n = std::min(5, (int)sorted.size());
        for (int i = 0; i < n; i++) {
            if (i) ss << ",";
            ss << "{\"q\":\"" << je(sorted[i]->pytanie) << "\",\"pct\":" << easeNaProcent(sorted[i]->poziomTrudnosci) << "}";
        }
        ss << "],\"categories\":[";
        bool first = true;
        for (const auto& kv : cats) {
            if (!first) ss << ",";
            ss << "{\"name\":\"" << je(kv.first) << "\",\"count\":" << kv.second << "}";
            first = false;
        }
        ss << "]}";
        return ss.str();
    });

    // ── pobierzDeckowCpm() — list all deck files ──────────────────────────
    w.bind("pobierzDeckowCpm", [](const std::string&) -> std::string {
        auto decks = listujTalie();
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < decks.size(); ++i) {
            if (i) ss << ",";
            ss << "{\"path\":\"" << je(decks[i].path) << "\""
               << ",\"name\":\"" << je(decks[i].name) << "\""
               << ",\"active\":" << (decks[i].path == g_sciezkaTalii ? "true" : "false") << "}";
        }
        ss << "]";
        return ss.str();
    });

    // ── pobierzKartyCpm({path}) — get all cards for the given deck path ────
    w.bind("pobierzKartyCpm", [](const std::string& req) -> std::string {
        std::string path = wyciagnijZJson(req, "path");
        const std::vector<Fiszka>* cards = &g_fiszki;
        std::vector<Fiszka> tmpDeck;
        if (!path.empty() && path != g_sciezkaTalii) {
            // Load a non-active deck temporarily without disturbing the session
            std::string savedPath = g_sciezkaTalii;
            std::string savedName = g_deckName;
            g_sciezkaTalii = path;
            tmpDeck = wczytajTalie();
            g_sciezkaTalii = savedPath;
            g_deckName     = savedName;
            cards = &tmpDeck;
        }
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < cards->size(); ++i) {
            if (i) ss << ",";
            ss << "{\"pytanie\":\""  << je((*cards)[i].pytanie)   << "\""
               << ",\"odpowiedz\":\"" << je((*cards)[i].odpowiedz) << "\""
               << ",\"etykieta\":\""  << je((*cards)[i].etykieta)  << "\"}";
        }
        ss << "]";
        return ss.str();
    });

    // ── usunFiszkeCpm({index}) — remove a flashcard from the active deck ──
    w.bind("usunFiszkeCpm", [](const std::string& req) -> std::string {
        std::string idxStr = wyciagnijZJson(req, "index");
        if (idxStr.empty()) return "{\"ok\":false,\"error\":\"Brak indeksu\"}";
        int idx;
        try { idx = std::stoi(idxStr); } catch (...) { return "{\"ok\":false}"; }
        if (idx < 0 || idx >= (int)g_fiszki.size())
            return "{\"ok\":false,\"error\":\"Zly indeks\"}";
        g_fiszki.erase(g_fiszki.begin() + idx);
        zapiszTalie(g_fiszki);
        g_poprzedniIndeks = -1;
        std::stringstream ss;
        ss << "{\"ok\":true,\"total\":" << g_fiszki.size() << "}";
        return ss.str();
    });

    // ── edytujFiszkeCpm({index, question, answer, label}) ─────────────────
    w.bind("edytujFiszkeCpm", [](const std::string& req) -> std::string {
        std::string idxStr = wyciagnijZJson(req, "index");
        std::string q      = wyciagnijZJson(req, "question");
        std::string a      = wyciagnijZJson(req, "answer");
        std::string l      = wyciagnijZJson(req, "label");
        if (idxStr.empty() || q.empty() || a.empty())
            return "{\"ok\":false,\"error\":\"Brakujace pola\"}";
        int idx;
        try { idx = std::stoi(idxStr); } catch (...) { return "{\"ok\":false}"; }
        if (idx < 0 || idx >= (int)g_fiszki.size())
            return "{\"ok\":false,\"error\":\"Zly indeks\"}";
        g_fiszki[idx].pytanie   = q;
        g_fiszki[idx].odpowiedz = a;
        g_fiszki[idx].etykieta  = l;
        zapiszTalie(g_fiszki);
        return "{\"ok\":true}";
    });

    // ── usunTalieCpm({path}) — delete a deck file ─────────────────────────
    w.bind("usunTalieCpm", [](const std::string& req) -> std::string {
        std::string path = wyciagnijZJson(req, "path");
        if (path.empty()) return "{\"ok\":false,\"error\":\"Brak sciezki\"}";
        try { std::filesystem::remove(path); } catch (...) {
            return "{\"ok\":false,\"error\":\"Blad usuwania\"}";
        }
        if (path == g_sciezkaTalii) {
            g_fiszki.clear();
            g_deckName = "";
            auto decks = listujTalie();
            if (!decks.empty()) {
                g_sciezkaTalii = decks[0].path;
                g_fiszki = wczytajTalie();
            } else {
                g_sciezkaTalii = "";
            }
        }
        return "{\"ok\":true}";
    });

    // ── wybranzTalieCpm({path}) — switch active deck ──────────────────────
    w.bind("wybranzTalieCpm", [](const std::string& req) -> std::string {
        std::string path = wyciagnijZJson(req, "path");
        if (path.empty()) return "{\"ok\":false,\"error\":\"Brak sciezki\"}";
        g_sciezkaTalii = path;
        g_deckName = "";
        g_fiszki = wczytajTalie();
        g_poprzedniIndeks = -1;
        std::stringstream ss;
        ss << "{\"ok\":true,\"total\":" << g_fiszki.size()
           << ",\"name\":\"" << je(g_deckName) << "\"}";
        return ss.str();
    });

    // ── stworzTalieCpm({filename, name}) — create new deck file ──────────
    w.bind("stworzTalieCpm", [](const std::string& req) -> std::string {
        std::string filename = wyciagnijZJson(req, "filename");
        std::string name     = wyciagnijZJson(req, "name");
        if (filename.empty() || name.empty())
            return "{\"ok\":false,\"error\":\"Nazwa i plik sa wymagane\"}";
        // Ensure .json extension
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json")
            filename += ".json";
        bool ok = stworzTalie(filename, name);
        if (!ok) return "{\"ok\":false,\"error\":\"Plik juz istnieje lub blad zapisu\"}";
        return "{\"ok\":true}";
    });

    // ── openLinkCpm({url}) — open external URL in system browser ─────────
    w.bind("openLinkCpm", [](const std::string& req) -> std::string {
        std::string url = wyciagnijZJson(req, "url");
        if (!url.empty()) {
            GError* err = nullptr;
            g_app_info_launch_default_for_uri(url.c_str(), nullptr, &err);
            if (err) g_error_free(err);
        }
        return "{\"ok\":true}";
    });

    w.navigate("app://localhost/");
    w.run();
    return 0;
}

