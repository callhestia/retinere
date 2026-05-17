// tests/test_engine.cpp — testy jednostkowe silnika Retinere
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

#include "../src/engine/engine.hpp"
#include "../src/engine/Levenshtein.hpp"

static int passed = 0, failed = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { \
            ++passed; \
        } else { \
            ++failed; \
            std::cerr << "[FAIL] " << msg \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } \
    } while (0)

// ---- SM-2 tests ----

static void test_sm2_ocena5() {
    double ef = zaplanujPowtorke(5, 2.5);
    CHECK(ef > 2.5, "ocena 5 zwieksza wspolczynnik");
}

static void test_sm2_ocena0() {
    double ef = zaplanujPowtorke(0, 2.5);
    CHECK(std::abs(ef - 2.5) < 1e-9, "ocena 0 nie zmienia wspolczynnika");
}

static void test_sm2_clamp_min() {
    double ef = zaplanujPowtorke(0, 1.0);
    CHECK(ef >= 1.3, "wspolczynnik nie spada ponizej 1.3");
}

static void test_sm2_clamp_max() {
    double ef = zaplanujPowtorke(5, 4.0);
    CHECK(ef <= 3.5, "wspolczynnik nie przekracza 3.5");
}

// ---- Levenshtein tests ----

static void test_lev_identical() {
    CHECK(obliczOdleglosc("kot", "kot") == 0, "identyczne slowa, odleglosc=0");
}

static void test_lev_one_insert() {
    CHECK(obliczOdleglosc("kat", "katt") == 1, "jedno wstawienie");
}

static void test_lev_empty() {
    CHECK(obliczOdleglosc("", "abc") == 3, "pusty vs 3-literowy");
}

static void test_typo_strict() {
    // identyczne slowa zawsze przechodzi, nawet w trybie scislym
    CHECK(czyBladDopuszczalny("hello", "hello", 0.0f), "identyczne = ok w strict");
}

static void test_typo_normal_fail() {
    // "helo" vs "hello": distance=1, max=5, ratio=0.2 > 0.1 => FAIL
    CHECK(!czyBladDopuszczalny("helo", "hello", 0.1f), "za duzo roznic w normalnym trybie");
}

static void test_typo_forgiving_pass() {
    // "helo" vs "hello": ratio=0.2 <= 0.25 => PASS
    CHECK(czyBladDopuszczalny("helo", "hello", 0.25f), "ok w ulgowym trybie");
}

// ---- obliczStatusOdpowiedzi tests ----

static void test_status_idealnie() {
    Fiszka f;
    f.pytanie = "q"; f.odpowiedz = "test"; f.poziomTrudnosci = 2.5;
    CHECK(obliczStatusOdpowiedzi("test", f, 0.1f) == 0, "idealna odpowiedz = 0");
}

static void test_status_literowka() {
    Fiszka f;
    f.pytanie = "q"; f.odpowiedz = "hello"; f.poziomTrudnosci = 2.5;
    CHECK(obliczStatusOdpowiedzi("helo", f, 0.25f) == 1, "mala literowka = 1");
}

static void test_status_blad() {
    Fiszka f;
    f.pytanie = "q"; f.odpowiedz = "hello"; f.poziomTrudnosci = 2.5;
    CHECK(obliczStatusOdpowiedzi("xyz", f, 0.1f) == 2, "bledna odpowiedz = 2");
}

// ---- progOdTrybu tests ----

static void test_threshold_strict() {
    CHECK(progOdTrybu(TRYB_DOKLADNY) == LEVENSHTEIN_DOKLADNY, "strict => 0.0");
}

static void test_threshold_normal() {
    CHECK(progOdTrybu(TRYB_NORMALNY) == LEVENSHTEIN_PROG, "normal => 0.1");
}

static void test_threshold_forgiving() {
    CHECK(progOdTrybu(TRYB_ULGOWY) == LEVENSHTEIN_ULGOWY, "forgiving => 0.25");
}

int main() {
    test_sm2_ocena5();
    test_sm2_ocena0();
    test_sm2_clamp_min();
    test_sm2_clamp_max();
    test_lev_identical();
    test_lev_one_insert();
    test_lev_empty();
    test_typo_strict();
    test_typo_normal_fail();
    test_typo_forgiving_pass();
    test_status_idealnie();
    test_status_literowka();
    test_status_blad();
    test_threshold_strict();
    test_threshold_normal();
    test_threshold_forgiving();

    std::cout << "\nWyniki: " << passed << " zaliczone";
    if (failed > 0) std::cout << ", " << failed << " NIEUDANE";
    std::cout << "\n";
    return (failed == 0) ? 0 : 1;
}
