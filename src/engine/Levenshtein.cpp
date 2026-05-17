#include "Levenshtein.hpp"
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

int obliczOdleglosc(string slowo1, string slowo2) {

    int dlugoscA = static_cast<int>(slowo1.length());
    int dlugoscB = static_cast<int>(slowo2.length());

        if (dlugoscA == 0) {
            return dlugoscB;
        }

        if (dlugoscB == 0) {
            return dlugoscA;
        }

    vector<vector<int>> tabela(dlugoscA + 1, vector<int>(dlugoscB + 1, 0));

    for (int i = 0; i <= dlugoscA; ++i) {
        
        tabela[i][0] = i; // usuwanie
    }

    for (int j = 0; j <= dlugoscB; ++j){

        tabela[0][j] = j; // wstawianie
    }

    for (int i = 1; i <= dlugoscA; ++i) {

        for (int j = 1; j <= dlugoscB; ++j) {

            bool znakyIdentyczne = slowo1[i - 1] == slowo2[j - 1];

            int usun = tabela[i - 1][j]+ 1;
            int wstaw = tabela[i][j - 1] + 1;
            int zamien = tabela[i - 1][j - 1] + (znakyIdentyczne ? 0 : 1);

            tabela[i][j] = min({ usun, wstaw, zamien });
        }
    }

    return tabela[dlugoscA][dlugoscB];
}

bool czyBladDopuszczalny(string slowo1, string slowo2, float prog) {
    int najdluzszeSlowo = static_cast<int>(max(slowo1.length(), slowo2.length()));

    if (najdluzszeSlowo == 0) {
        
        return true;
    }

    float wspolczynnikBledu = static_cast<float>(obliczOdleglosc(slowo1, slowo2))

                            / static_cast<float>(najdluzszeSlowo);

    return wspolczynnikBledu <= prog;
}
