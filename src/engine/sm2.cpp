#include "engine.hpp"
using namespace std;

double zaplanujPowtorke(int ocena, double poprzedniWspolczynnik) {

    double wspolczynnik = 0.0;

    if (ocena == 0) {
        wspolczynnik = poprzedniWspolczynnik;
    } else if (ocena > 3) {
        wspolczynnik = poprzedniWspolczynnik + (0.1 - (5 - ocena) * (0.08 + (5 - ocena) * 0.02));
    } else {
        wspolczynnik = poprzedniWspolczynnik - 0.15 - (3 - ocena) * 0.05;
    }



    if (wspolczynnik < 1.3) {
        wspolczynnik = 1.3;
    }

    
    if (wspolczynnik > 3.5) {
        wspolczynnik = 3.5;
    }

    return wspolczynnik;
}