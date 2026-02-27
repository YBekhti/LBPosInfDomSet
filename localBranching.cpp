/************************************************************
               lb_pids.cpp - Local Branching for PIDS
 ***********************************************************/

using namespace std;

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <random>
#include <chrono>
#include <ctime>
// #include <ilcplex/ilocplex.h>

// ILOSTLBEGIN

// Structures de données
struct Solution {
    set<int> vertices;
    int score;

    Solution() : score(0) {}
};

// Variables globales pour l'algorithme


// Variables globales pour l'algorithme
double temps_total_limit = 1800.0;
double temps_intensification_limit = 500.0;
double temps_diversification_limit = 100.0;
double alpha = 0.3;  // 30% pour la perturbation
double param_beta = 0.4;   // 40% pour la destruction
int k = 20;           // Distance de Hamming pour le voisinage
int age_limit = 10;

// AJOUTEZ CES 3 LIGNES ICI :
bool warm_start = true;
bool cplex_abort = false;
bool heuristic_emphasis = false;

// Variables pour le graphe
int n_sommets;
vector<set<int>> voisins;
//vector<int> b, g;



//*******************************************************************
//****************************************************************

// ====================================================
// FONCTIONS DE CONVERSION MATRICE -> LISTE D'ARÊTES
// ====================================================

/*
 * Fonction pour convertir une matrice d'adjacence en liste d'arêtes
 * Format d'entrée : matrice d'adjacence (0/1)
 * Format de sortie : nombre de sommets, nombre d'arêtes, puis liste des arêtes
 * Fichier de sortie : "output_" + nom_fichier_entree
 */
bool convertir_matrice_vers_liste_aretes(const string& nom_fichier_entree) {

    ifstream fichier_in(nom_fichier_entree.c_str());
    if (!fichier_in) {
        cout << "Erreur: impossible d'ouvrir le fichier " << nom_fichier_entree << endl;
        return false;
    }

    // Lire la matrice d'adjacence
    vector<vector<int>> matrice;
    string ligne;

    // Lire toutes les lignes du fichier
    while (getline(fichier_in, ligne)) {
        // Ignorer les lignes vides
        if (ligne.empty()) continue;

        istringstream iss(ligne);
        vector<int> ligne_matrice;
        int valeur;

        while (iss >> valeur) {
            ligne_matrice.push_back(valeur);
        }

        if (!ligne_matrice.empty()) {
            matrice.push_back(ligne_matrice);
        }
    }
    fichier_in.close();

    // Vérifier que la matrice n'est pas vide
    if (matrice.empty()) {
        cout << "Erreur: matrice vide" << endl;
        return false;
    }

    int n_sommets = matrice.size();

    // Vérifier que la matrice est carrée
    for (int i = 0; i < n_sommets; ++i) {
        if (matrice[i].size() != n_sommets) {
            cout << "Erreur: la matrice n'est pas carrée (ligne " << i
                 << " a " << matrice[i].size() << " éléments au lieu de "
                 << n_sommets << ")" << endl;
            return false;
        }
    }

    // Compter les arêtes (ne compter que la moitié supérieure pour éviter les doublons)
    int n_aretes = 0;
    vector<pair<int, int>> aretes;

    for (int i = 0; i < n_sommets; ++i) {
        for (int j = i + 1; j < n_sommets; ++j) {  // j = i+1 pour éviter les doublons
            if (matrice[i][j] != 0) {
                // Ajouter 1 aux indices pour commencer à 1 (pas à 0)
                aretes.push_back(make_pair(i + 1, j + 1));
                n_aretes++;
            }
        }
    }

    // Créer le nom du fichier de sortie
    string nom_sortie = "output_" + nom_fichier_entree;

    // Écrire dans le fichier de sortie
    ofstream fichier_out(nom_sortie.c_str());
    if (!fichier_out) {
        cout << "Erreur: impossible de créer le fichier " << nom_sortie << endl;
        return false;
    }

    // Écrire l'en-tête
    fichier_out << n_sommets << endl;
    fichier_out << n_aretes << endl;

    // Écrire les arêtes
    for (const auto& arete : aretes) {
        fichier_out << arete.first << " " << arete.second << endl;
    }

    fichier_out.close();

    // Afficher un résumé
    cout << "========================================" << endl;
    cout << "CONVERSION REUSSIE" << endl;
    cout << "Fichier d'entrée  : " << nom_fichier_entree << endl;
    cout << "Fichier de sortie : " << nom_sortie << endl;
    cout << "Nombre de sommets : " << n_sommets << endl;
    cout << "Nombre d'arêtes   : " << n_aretes << endl;
    cout << "========================================" << endl;

    return true;
}

/*
 * Fonction pour afficher l'aide du programme
 */
void afficher_aide() {
    cout << "========================================" << endl;
    cout << "LOCAL BRANCHING POUR PIDS" << endl;
    cout << "========================================" << endl;
    cout << "Usage :" << endl;
    cout << "  ./lb_pids -i <fichier_liste_aretes> [options]" << endl;
    cout << "     Exécute l'algorithme sur un fichier liste d'arêtes" << endl;
    cout << endl;
    cout << "  ./lb_pids -c <fichier_matrice>" << endl;
    cout << "     Convertit une matrice d'adjacence en liste d'arêtes" << endl;
    cout << endl;
    cout << "Options :" << endl;
    cout << "  -t  <double> : Temps total limite (defaut: 100.0)" << endl;
    cout << "  -ti <double> : Temps intensification limite (defaut: 10.0)" << endl;
    cout << "  -td <double> : Temps diversification limite (defaut: 10.0)" << endl;
    cout << "  -a  <double> : Alpha (perturbation %) (defaut: 0.3)" << endl;
    cout << "  -b  <double> : Beta (destruction %) (defaut: 0.4)" << endl;
    cout << "  -k  <int>    : K (Distance Hamming) (defaut: 2)" << endl;
    cout << "========================================" << endl;
}

//*************************************************************
//***************************************************************



/* Fonction pour lire le graphe depuis un fichier txt */


/* Fonction pour lire le graphe depuis un fichier txt */
void lire_graphe(const string& nom_fichier, int& n_sommets,
                 vector<set<int>>& voisins) {

    ifstream fichier(nom_fichier.c_str());
    if (!fichier) {
        cout << "Erreur: fichier impossible a ouvrir" << endl;
        exit(1);
    }

    int m_aretes;
    // Read N and M (though we mostly need N and the matrix)
    if (!(fichier >> n_sommets >> m_aretes)) {
        cout << "Erreur de lecture de n et m" << endl;
        exit(1);
    }

    voisins = vector<set<int>>(n_sommets);

    // Read Adjacency Matrix
    int val;
    for (int i = 0; i < n_sommets; ++i) {
        for (int j = 0; j < n_sommets; ++j) {
            fichier >> val;
            if (val == 1 && i != j) {
                voisins[i].insert(j);
                voisins[j].insert(i);
            }
        }
    }

    fichier.close();
}












/* Fonction CPLEX pour résoudre le problème */

// CPLEX Executable Path - cross-platform with multi-path discovery
string getCplexPath() {
    // 1. Check environment variable first (set by run_comparison.sh)
    const char* env_p = getenv("CPLEX_BIN");
    if (env_p && env_p[0] != '\0') return string(env_p);

    #ifdef _WIN32
        return "\"C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio128\\cplex\\bin\\x64_win64\\cplex.exe\"";
    #else
        // 2. Search known Linux CPLEX installation paths (same as compile.sh)
        const char* home = getenv("HOME");
        const string homeStr = home ? string(home) : "";
        const string arch = "x86-64_linux";
        const vector<string> basePaths = {
            homeStr + "/YaminaBNBCPLEX/cplex_studio_1210",
            "/opt/ibm/ILOG/CPLEX_Studio1210",
            "/opt/ibm/ILOG/CPLEX_Studio129",
            "/opt/ibm/ILOG/CPLEX_Studio128",
            "/usr/local/ibm/ILOG/CPLEX_Studio1210",
            homeStr + "/ibm/ILOG/CPLEX_Studio1210"
        };
        for (const string& base : basePaths) {
            string candidate = base + "/cplex/bin/" + arch + "/cplex";
            if (FILE* f = fopen(candidate.c_str(), "r")) {
                fclose(f);
                return candidate;
            }
        }
        // 3. Last resort: hope cplex is in PATH
        return "cplex";
    #endif
}

const string CPLEX_PATH = getCplexPath();

void run_cplex(Solution& cpl_sol, Solution& best_sol, vector<int>& age, double r_limit) {
    // 0. Generate MST file (Warm Start)
    if (warm_start) {
        ofstream mstFile("start.mst");
        if (mstFile) {
            mstFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
            mstFile << "<CPLEXSolutions>" << endl;
            mstFile << " <CPLEXSolution version=\"1.0\">" << endl;
            mstFile << "  <header/>" << endl;
            mstFile << "  <variables>" << endl;
            
            for (int i = 0; i < n_sommets; ++i) {
                // If strict compatibility is required:
                // Only provide value if it doesn't conflict with age (fixed to 0)
                // However, CPLEX can usually handle partial or repairing. 
                // Let's provide 1s for vertices in best_sol, unless fixed to 0.
                
                if (age[i] == -1) {
                    mstFile << "   <variable name=\"x" << (i+1) << "\" value=\"0\"/>" << endl;
                } else {
                    if (best_sol.vertices.find(i) != best_sol.vertices.end()) {
                        mstFile << "   <variable name=\"x" << (i+1) << "\" value=\"1\"/>" << endl;
                    } else {
                        mstFile << "   <variable name=\"x" << (i+1) << "\" value=\"0\"/>" << endl;
                    }
                }
            }
            
            mstFile << "  </variables>" << endl;
            mstFile << " </CPLEXSolution>" << endl;
            mstFile << "</CPLEXSolutions>" << endl;
            mstFile.close();
        }
    }

    // 1. Generate LP file
    string lpFilename = "subproblem.lp";
    ofstream lpFile(lpFilename.c_str());
    if (!lpFile) { cout << "Error creating LP file" << endl; return; }

    lpFile << "Minimize" << endl << " obj: ";
    for (int i = 0; i < n_sommets; ++i) {
        lpFile << "x" << (i+1);
        if (i < n_sommets-1) lpFile << " + ";
    }
    lpFile << endl << "Subject To" << endl;

    // Constraints
    for (int i = 0; i < n_sommets; ++i) {
        double rhs_val = ceil(voisins[i].size() * 0.5);
        int rhs = (int)rhs_val;

        lpFile << " c" << (i+1) << ": ";
        for (set<int>::iterator sit = voisins[i].begin(); sit != voisins[i].end(); ++sit) {
            lpFile << "x" << (*sit + 1) << " + ";
        }
        lpFile << rhs << " x" << (i+1) << " >= " << rhs << endl; 
    }
    
    // Fixing variables based on age
    // If age is -1, fix to 0 (Exclude)
    // If age is 1, fix to 1 (Keep / Lower Bound = 1)
    // If age is 0, free (Binary)
    
    lpFile << "Bounds" << endl;
    for (int i = 0; i < n_sommets; ++i) {
        if (age[i] == -1) {
            lpFile << " x" << (i+1) << " = 0" << endl;
        } else if (age[i] == 1) {
             lpFile << " x" << (i+1) << " = 1" << endl;
        } else {
             lpFile << " 0 <= x" << (i+1) << " <= 1" << endl;
        }
    }

    lpFile << "Binaries" << endl;
    for (int i=0; i<n_sommets; ++i) lpFile << " x" << (i+1) << endl;
    lpFile << "End" << endl;
    lpFile.close();

    // 2. Generate Script
    ofstream script("script_sub.txt");
    script << "read " << lpFilename << endl;
    if (warm_start) {
        script << "read start.mst" << endl;
    }
    script << "set timelimit " << r_limit << endl;
    script << "set emphasis mip 1" << endl; // Feasibility emphasis
    script << "optimize" << endl;
    script << "display solution variables -" << endl;
    script << "quit" << endl;
    script.close();

    // 3. Run CPLEX
    string cmd = CPLEX_PATH + " -f script_sub.txt > cplex_sub.log";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        cout << "DEBUG: CPLEX command failed with return code " << ret << endl;
    } else {
        cout << "DEBUG: CPLEX run completed. Checking log..." << endl;
    }

    // 4. Parse Output
    cpl_sol.score = std::numeric_limits<int>::max(); // Default to infinite/invalid
    cpl_sol.vertices.clear();

    ifstream log("cplex_sub.log");
    string line;
    bool readingSol = false;
    bool foundHeaders = false;
    vector<int> sol_vars;
    
    // DEBUG: Read first few lines of log to verify content
    /*
    ifstream debug_log("cplex_sub.log");
    string debug_line;
    int debug_count = 0;
    cout << "--- CPLEX LOG START ---" << endl;
    while(getline(debug_log, debug_line) && debug_count < 5) {
        cout << debug_line << endl;
        debug_count++;
    }
    cout << "--- CPLEX LOG END ---" << endl;
    debug_log.close();
    */
    
    if (log.is_open()) {
        while(getline(log, line)) {
             if (line.find("Variable Name") != string::npos) {
                 readingSol = true;
                 foundHeaders = true;
             }
             if (readingSol) {
                 size_t x_pos = line.find("x");
                 if (x_pos != string::npos) {
                     // Check if it's actually a variable line (starts with x or has space before x)
                     size_t space = line.find(" ", x_pos);
                     if (space != string::npos) {
                         string numStr = line.substr(x_pos+1, space - (x_pos+1));
                         int idx = atoi(numStr.c_str()) - 1;
                         
                         string valStr = line.substr(space);
                         size_t firstNonSpace = valStr.find_first_not_of(" \t");
                         if(firstNonSpace != string::npos) {
                             double val = atof(valStr.c_str() + firstNonSpace);
                             if (val > 0.5) { // If value is 1
                                 sol_vars.push_back(idx);
                             }
                         }
                     }
                 }
                 if (line.find("CPLEX>") != string::npos) readingSol = false;
             }
        }
        log.close();
    } else {
        cout << "DEBUG: Could not open cplex_sub.log!" << endl;
    }

    cout << "DEBUG: Parsed " << sol_vars.size() << " variables from CPLEX output." << endl;

    // Only update solution if we actually found the variables section
    if (foundHeaders) {
        cpl_sol.score = 0;
        for (int i=0; i<n_sommets; ++i) {
            bool is_one = false; 
            for(size_t k=0; k<sol_vars.size(); ++k) {
                 if(sol_vars[k]==i) { is_one=true; break; }
        }
            
            if (age[i] >= 0) {
                age[i]++;
                if (is_one) {
                    cpl_sol.score++;
                    age[i] = 0;
                    cpl_sol.vertices.insert(i);
                }
                if (age[i] >= age_limit) age[i] = -1;
            }
        }
        cout << "solution cplex " << cpl_sol.score << endl;
    } else {
        // Fallback: CPLEX failed (infeasible or validation failed or time limit reached without solution)
        // We incorrectly told the user it's logic to return the initial solution if no better one found.
        // Returning the 'best_sol' (which was our warm start) is a safe fallback for the ALGORITHM flow,
        // effectively meaning "no improvement".
        
        cout << "solution cplex FAILED (using previous best: " << best_sol.score << ")" << endl;
        cpl_sol = best_sol; // Copy previous best
    }
}












/* Fonction pour calculer la distance de Hamming entre deux solutions */
int distance_hamming(const Solution& s1, const Solution& s2) {
    int distance = 0;

    for (int v : s1.vertices) {
        if (s2.vertices.find(v) == s2.vertices.end()) {
            distance++;
        }
    }

    for (int v : s2.vertices) {
        if (s1.vertices.find(v) == s1.vertices.end()) {
            distance++;
        }
    }

    return distance;
}

/* Fonction pour vérifier si un sommet est dominé par une solution */


/* Fonction pour vérifier si un sommet est dominé par une solution */
bool est_domine(int sommet, const Solution& solution) {
    // Si le sommet est dans la solution, il est toujours dominé
    if (solution.vertices.find(sommet) != solution.vertices.end()) {
        return true;
    }

    // Sinon, il doit avoir au moins la moitié de ses voisins dans la solution
    int total_voisins = voisins[sommet].size();
    int seuil = ceil(total_voisins * 0.5);  // Au moins la moitié arrondie à l'entier supérieur

    int compteur = 0;
    for (int voisin : voisins[sommet]) {
        if (solution.vertices.find(voisin) != solution.vertices.end()) {
            compteur++;
        }
    }

    return compteur >= seuil;
}








/* Fonction pour calculer le score d'une solution */
int calculer_score(const Solution& solution) {
    int score = 0;
    for (int v : solution.vertices) {
        score +=1;//ici
    }
    return score;
}

/* Fonction pour supprimer aléatoirement un pourcentage des sommets */
set<int> supprimer_pourcentage(const Solution& solution, double pourcentage,
                              default_random_engine& generator,
                              uniform_real_distribution<double>& distribution) {

    set<int> sommets_supprimes;
    vector<int> sommets_solution(solution.vertices.begin(), solution.vertices.end());

    int n_a_supprimer = static_cast<int>(sommets_solution.size() * pourcentage);
    if (n_a_supprimer == 0 && !sommets_solution.empty()) {
        n_a_supprimer = 1;
    }

    for (int i = 0; i < n_a_supprimer; ++i) {
        double r = distribution(generator);
        int idx = static_cast<int>(r * sommets_solution.size());
        if (idx == sommets_solution.size()) idx = sommets_solution.size() - 1;

        sommets_supprimes.insert(sommets_solution[idx]);
        sommets_solution.erase(sommets_solution.begin() + idx);
    }

    return sommets_supprimes;
}

/* Heuristique de roulette pour générer une solution initiale */


/* Heuristique de roulette pour générer une solution initiale */
Solution heuristique_roulette_exacte(int n_sommets, vector<set<int>>& voisins,
                                     default_random_engine& generator,
                                     uniform_real_distribution<double>& distribution) {

    Solution x0;
    x0.score = 0;

    // Ensemble des sommets non encore dominés
    set<int> C;
    for (int i = 0; i < n_sommets; ++i) {
        C.insert(i);
    }

    // Calculer les degrés pour la roulette
    vector<int> deg_G(n_sommets, 0);
    int somme_degres = 0;

    for (int i = 0; i < n_sommets; ++i) {
        deg_G[i] = voisins[i].size();
        somme_degres += deg_G[i];
    }

    // Éviter division par zéro pour graphes sans arêtes
    if (somme_degres == 0) {
        // Si pas d'arêtes, chaque sommet doit être dans la solution
        for (int i = 0; i < n_sommets; ++i) {
            x0.vertices.insert(i);
        }
        x0.score = n_sommets;  // Cardinal de la solution
        return x0;
    }

    // Probabilités proportionnelles aux degrés
    vector<double> p(n_sommets, 0.0);
    for (int i = 0; i < n_sommets; ++i) {
        p[i] = static_cast<double>(deg_G[i]) / somme_degres;
    }

    // Trier les sommets par probabilité
    vector<pair<int, double>> sommets_tries(n_sommets);
    for (int i = 0; i < n_sommets; ++i) {
        sommets_tries[i] = {i, p[i]};
    }

    sort(sommets_tries.begin(), sommets_tries.end(),
         [](const pair<int, double>& a, const pair<int, double>& b) {
             return a.second < b.second;
         });

    // Créer les intervalles pour la roulette
    double l = 0.0;
    vector<pair<double, double>> intervalles(n_sommets);

    for (int i = 0; i < n_sommets; ++i) {
        double longueur_intervalle = sommets_tries[i].second;
        intervalles[i] = {l, l + longueur_intervalle};
        l += longueur_intervalle;
    }

    // Pour chaque sommet, stocker combien de voisins doivent être dans la solution
    // si le sommet n'est pas dans la solution (besoin = ceil(deg(i)/2))
    vector<int> besoins(n_sommets, 0);
    for (int i = 0; i < n_sommets; ++i) {
        besoins[i] = ceil(voisins[i].size() * 0.5);
    }

    while (!C.empty()) {
        double p_r = distribution(generator);

        // Sélectionner un sommet avec la roulette
        int sommet_selectionne = -1;
        for (int i = 0; i < n_sommets; ++i) {
            if (intervalles[i].first <= p_r && p_r < intervalles[i].second) {
                sommet_selectionne = sommets_tries[i].first;
                break;
            }
        }

        // Si pas trouvé (peut arriver à cause d'erreurs d'arrondi)
        if (sommet_selectionne == -1) {
            sommet_selectionne = sommets_tries.back().first;
        }

        // Si le sommet sélectionné n'est plus dans C, en prendre un aléatoire
        if (C.find(sommet_selectionne) == C.end()) {
            if (!C.empty()) {
                double r = distribution(generator);
                int pos = static_cast<int>(r * C.size());
                if (pos == C.size()) pos = C.size() - 1;

                auto it = C.begin();
                advance(it, pos);
                sommet_selectionne = *it;
            }
        }

        if (sommet_selectionne == -1) break;

        // Ajouter le sommet à la solution
        x0.vertices.insert(sommet_selectionne);
        x0.score += 1;  // Score = cardinal de la solution

        // Mettre à jour les besoins
        // Si le sommet est dans la solution, il est dominé
        besoins[sommet_selectionne] = 0;

        // Pour chaque voisin, réduire son besoin de 1
        for (int voisin : voisins[sommet_selectionne]) {
            if (besoins[voisin] > 0) {
                besoins[voisin]--;
            }
        }

        // Mettre à jour l'ensemble C (sommets non encore dominés)
        set<int> nouveaux_domines;

        // Vérifier si le sommet sélectionné est maintenant dominé
        if (besoins[sommet_selectionne] == 0) {
            nouveaux_domines.insert(sommet_selectionne);
        }

        // Vérifier les voisins
        for (int voisin : voisins[sommet_selectionne]) {
            if (besoins[voisin] == 0) {
                nouveaux_domines.insert(voisin);
            }
        }

        // Retirer les sommets dominés de C
        C.erase(sommet_selectionne);
        for (int sommet : nouveaux_domines) {
            C.erase(sommet);
        }

        // Vérifier si tous les sommets sont dominés
        bool tous_domines = true;
        for (int i = 0; i < n_sommets; ++i) {
            if (besoins[i] > 0) {
                tous_domines = false;
                break;
            }
        }

        if (tous_domines) {
            break;
        }
    }

    return x0;
}
















/* Phase d'intensification */

/* Phase d'intensification utilisant run_cplex avec Alpha controlé */
Solution phase_intensification_avec_run_cplex(Solution solution_courante, double temps_limit) {

    clock_t debut = clock();
    Solution meilleure_solution = solution_courante;

    // 1. Définir l'espace de recherche (voisinage)
    vector<int> age(n_sommets, -1);  // -1 = sommet fixé à 0, 0 = sommet libre (binaire)



    // A. Traitement des sommets de la solution : Sélectionner EXACTEMENT alpha % à détruire
    vector<int> sommets_sol;
    for (int v : solution_courante.vertices) {
        sommets_sol.push_back(v);
    }

    // Mélanger aléatoirement les indices
    // Utilisation de rand() pour le mélange simple
    for (size_t i = 0; i < sommets_sol.size(); ++i) {
        size_t j = i + rand() % (sommets_sol.size() - i);
        swap(sommets_sol[i], sommets_sol[j]);
    }

    // Calculer le nombre de sommets à détruire (libérer)
    int n_a_detruire = (int)(sommets_sol.size() * alpha);
    
    // Les premiers n_a_detruire sont mis à 0 (Libre), les autres à 1 (Fixé)
    int n_fixed = 0;
    int n_free = 0;
    for (size_t i = 0; i < sommets_sol.size(); ++i) {
        int v = sommets_sol[i];
        if (i < n_a_detruire) {
            age[v] = 0; // Libre (Détruit/Re-optimisé)
            n_free++;
        } else {
            age[v] = 1; // Fixé à 1 (Gardé)
            n_fixed++;
        }
    }
    cout << "DEBUG: Alpha=" << alpha << " Total Sol=" << sommets_sol.size() 
         << " Destroyed(Free)=" << n_free << " Kept(Fixed)=" << n_fixed << endl;

    // B. Traitement des sommets HORS solution
    // On garde l'approche probabiliste pour l'expansion, ou on peut faire pareil
    // Ici, on garde l'approche probabiliste comme avant pour l'expansion
    for (int i = 0; i < n_sommets; ++i) {
        if (solution_courante.vertices.find(i) == solution_courante.vertices.end()) {
             double r = ((double) rand() / (RAND_MAX));
             if (r < alpha) {
                 age[i] = 0; // Libre (Expansion)
             } else {
                 age[i] = -1; // Fixé à 0 (Exclu)
             }
        }
    }

    // 2. Appeler CPLEX sur ce sous-problème
    Solution solution_cplex;
    double temps_restant = temps_limit;
    
    // Solution de référence pour le warm-start
    Solution reference_solution = solution_courante;

    run_cplex(solution_cplex, reference_solution, age, temps_restant);

    // 3. Vérifier et retourner
    bool solution_valide = true;
    for (int i = 0; i < n_sommets; ++i) {
        if (!est_domine(i, solution_cplex)) {
            solution_valide = false;
            break;
        }
    }

    if (solution_valide && solution_cplex.score < meilleure_solution.score) {
        return solution_cplex;
    }

    return meilleure_solution;
}






/* Phase de diversification */



/* Phase de diversification avec reconstruction intelligente */
Solution phase_diversification_avec_roulette(Solution solution_initiale, double temps_limit,
                                             default_random_engine& generator,
                                             uniform_real_distribution<double>& distribution) {

    clock_t debut = clock();
    Solution meilleure_solution = solution_initiale;

    while (true) {
        double temps_ecoule = double(clock() - debut) / CLOCKS_PER_SEC;
        if (temps_ecoule >= temps_limit) {
            break;
        }

        // 1. Destruction : supprimer une partie de la solution
        set<int> sommets_supprimes = supprimer_pourcentage(meilleure_solution, param_beta, generator, distribution);

        // 2. Garder le noyau (sommets non supprimés)
        set<int> noyau;
        for (int v : meilleure_solution.vertices) {
            if (sommets_supprimes.find(v) == sommets_supprimes.end()) {
                noyau.insert(v);
            }
        }

        // 3. Reconstruction avec l'heuristique de roulette sur le sous-problème
        // Créer un sous-graphe des sommets à considérer
        set<int> sommets_a_considerer;

        // Inclure les sommets supprimés
        sommets_a_considerer.insert(sommets_supprimes.begin(), sommets_supprimes.end());

        // Inclure les sommets non dominés par le noyau
        for (int i = 0; i < n_sommets; ++i) {
            Solution temp_solution;
            temp_solution.vertices = noyau;
            if (!est_domine(i, temp_solution)) {
                sommets_a_considerer.insert(i);
            }
        }

        // 4. Appliquer l'heuristique de roulette sur le sous-graphe
        if (!sommets_a_considerer.empty()) {
            // Créer une copie modifiée de l'heuristique pour travailler sur le sous-ensemble
            Solution solution_reconstruite;
            solution_reconstruite.vertices = noyau;  // Commencer avec le noyau
            solution_reconstruite.score = noyau.size();

            // Identifier les sommets non encore dominés
            set<int> C;  // Sommets non dominés
            for (int i = 0; i < n_sommets; ++i) {
                if (!est_domine(i, solution_reconstruite)) {
                    C.insert(i);
                }
            }

            // Calculer les besoins initiaux
            vector<int> besoins(n_sommets, 0);
            for (int i : C) {
                besoins[i] = ceil(voisins[i].size() * 0.5);
            }

            // Heuristique de roulette adaptée
            while (!C.empty()) {
                // Créer une distribution sur les sommets à considérer
                vector<int> candidats;
                for (int v : sommets_a_considerer) {
                    if (C.find(v) != C.end() ||
                        (solution_reconstruite.vertices.find(v) == solution_reconstruite.vertices.end() &&
                         sommets_a_considerer.find(v) != sommets_a_considerer.end())) {
                        candidats.push_back(v);
                    }
                }

                if (candidats.empty()) {
                    // Si plus de candidats, ajouter un sommet aléatoire pour dominer les restants
                    if (!C.empty()) {
                        auto it = C.begin();
                        advance(it, rand() % C.size());
                        int sommet_ajoute = *it;
                        solution_reconstruite.vertices.insert(sommet_ajoute);
                        solution_reconstruite.score++;

                        // Mettre à jour C et besoins
                        besoins[sommet_ajoute] = 0;
                        for (int voisin : voisins[sommet_ajoute]) {
                            if (besoins[voisin] > 0) {
                                besoins[voisin]--;
                                if (besoins[voisin] == 0) {
                                    C.erase(voisin);
                                }
                            }
                        }
                        C.erase(sommet_ajoute);
                    }
                    continue;
                }

                // Roulette : probabilité proportionnelle au degré
                int total_deg = 0;
                vector<int> deg_candidats(candidats.size());
                for (size_t i = 0; i < candidats.size(); ++i) {
                    deg_candidats[i] = voisins[candidats[i]].size();
                    total_deg += deg_candidats[i];
                }

                if (total_deg == 0) {
                    // Tous les sommets isolés
                    for (int v : candidats) {
                        solution_reconstruite.vertices.insert(v);
                        solution_reconstruite.score++;
                    }
                    break;
                }

                // Sélection par roulette
                double r = distribution(generator);
                double cumul = 0.0;
                int sommet_selectionne = candidats[0];

                for (size_t i = 0; i < candidats.size(); ++i) {
                    double proba = (double)deg_candidats[i] / total_deg;
                    cumul += proba;
                    if (r <= cumul) {
                        sommet_selectionne = candidats[i];
                        break;
                    }
                }

                // Ajouter le sommet sélectionné
                solution_reconstruite.vertices.insert(sommet_selectionne);
                solution_reconstruite.score++;

                // Mettre à jour C et besoins
                besoins[sommet_selectionne] = 0;
                for (int voisin : voisins[sommet_selectionne]) {
                    if (besoins[voisin] > 0) {
                        besoins[voisin]--;
                        if (besoins[voisin] == 0) {
                            C.erase(voisin);
                        }
                    }
                }
                C.erase(sommet_selectionne);
            }

            // 5. Vérifier et accepter la solution
            bool solution_valide = true;
            for (int i = 0; i < n_sommets; ++i) {
                if (!est_domine(i, solution_reconstruite)) {
                    solution_valide = false;
                    break;
                }
            }

            if (solution_valide && solution_reconstruite.score < meilleure_solution.score) {
                meilleure_solution = solution_reconstruite;
            }
        }
    }

    return meilleure_solution;
}



/* Algorithme principal de Local Branching */



/* Algorithme principal de Local Branching */
/* Algorithme principal de Local Branching */
Solution algorithme_local_branching(ofstream& logFile) {

    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(seed);
    uniform_real_distribution<double> distribution(0.0, 1.0);

    clock_t debut_total = clock();

    // Appel correct de l'heuristique
    Solution x0 = heuristique_roulette_exacte(n_sommets, voisins, generator, distribution);
    Solution x_bar = x0;

    cout << "Solution initiale: score = " << x0.score << endl;
    if (logFile.is_open()) logFile << "Solution initiale: score = " << x0.score << endl;

    while (true) {
        clock_t maintenant = clock();
        double temps_ecoule = double(maintenant - debut_total) / CLOCKS_PER_SEC;

        if (temps_ecoule >= temps_total_limit) {
            break;
        }

        // CORRECTION : utiliser la bonne fonction d'intensification
        Solution x_intensif = phase_intensification_avec_run_cplex(x_bar, temps_intensification_limit);

        int dist = distance_hamming(x_bar, x_intensif);

        if (x_intensif.score < x_bar.score) {
            cout << "Amelioration trouvee: " << x_intensif.score
                 << " (distance Hamming: " << dist << ")" << endl;
            if (logFile.is_open()) logFile << "Amelioration trouvee: " << x_intensif.score
                 << " (distance Hamming: " << dist << ")" << endl;

            if (dist <= k) {
                x_bar = x_intensif;
            } else {
                vector<int> age_local(n_sommets, -1);
                for (int v : x_intensif.vertices) {
                    age_local[v] = 0;
                }

                double temps_restant = temps_total_limit - temps_ecoule;
                if (temps_restant > 10.0) temps_restant = 10.0;

                Solution x_cplex;
                // CORRECTION : utiliser run_cplex au lieu de executer_cplex
                run_cplex(x_cplex, x_bar, age_local, temps_restant);

                // Note: run_cplex renvoie déjà une solution complète, pas besoin de fusionner
                // x_cplex.vertices.insert(x_bar.vertices.begin(), x_bar.vertices.end());
                x_cplex.score = calculer_score(x_cplex);

                if (x_cplex.score < x_bar.score) {
                    x_bar = x_cplex;
                    cout << "Nouvelle meilleure solution apres CPLEX: " << x_bar.score << endl;
                    if (logFile.is_open()) logFile << "Nouvelle meilleure solution apres CPLEX: " << x_bar.score << endl;
                }
            }
        } else {
            // CORRECTION : utiliser la bonne fonction de diversification
            Solution x_diversif = phase_diversification_avec_roulette(x_bar, temps_diversification_limit,
                                                                     generator, distribution);

            if (x_diversif.score < x_bar.score) {
                x_bar = x_diversif;
                cout << "Diversification reussie: " << x_bar.score << endl;
                if (logFile.is_open()) logFile << "Diversification reussie: " << x_bar.score << endl;
            }
        }

        maintenant = clock();
        temps_ecoule = double(maintenant - debut_total) / CLOCKS_PER_SEC;

        if (temps_ecoule >= temps_total_limit) {
            break;
        }
    }

    return x_bar;
}








/* Fonction principale */
/*int main(int argc, char** argv) {

    if (argc < 3) {
        cout << "Usage: " << argv[0] << " -i <fichier_entree>" << endl;
        return 1;
    }

    string fichier_entree;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            fichier_entree = argv[i + 1];
            break;
        }
    }

    if (fichier_entree.empty()) {
        cout << "Erreur: veuillez specifier un fichier d'entree avec -i" << endl;
        return 1;
    }

    lire_graphe(fichier_entree, n_sommets, voisins, b, g);

    cout << "========================================" << endl;
    cout << "Local Branching pour PIDS" << endl;
    cout << "Instance: " << fichier_entree << endl;
    cout << "Nombre de sommets: " << n_sommets << endl;
    cout << "Temps limite: " << temps_total_limit << " secondes" << endl;
    cout << "Parametres: alpha=" << alpha << ", beta=" << beta << ", k=" << k << endl;
    cout << "========================================" << endl;

    Solution meilleure_solution = algorithme_local_branching();

    cout << "\n=== RESULTATS FINAUX ===" << endl;
    cout << "Score optimal: " << meilleure_solution.score << endl;
    cout << "Nombre de sommets selectionnes: " << meilleure_solution.vertices.size() << endl;

    vector<int> sommets_tries(meilleure_solution.vertices.begin(),
                             meilleure_solution.vertices.end());
    sort(sommets_tries.begin(), sommets_tries.end());

    cout << "Sommets selectionnes: ";
    for (size_t i = 0; i < sommets_tries.size(); ++i) {
        cout << sommets_tries[i];
        if (i < sommets_tries.size() - 1) cout << " ";
    }
    cout << endl;

    // Verification de la solution
    bool solution_valide = true;
    for (int i = 0; i < n_sommets; ++i) {
        if (!est_domine(i, meilleure_solution)) {
            solution_valide = false;
            cout << "ERREUR: Sommet " << i << " non domine!" << endl;
            break;
        }
    }

    if (solution_valide) {
        cout << "Solution verifiee: tous les sommets sont domines." << endl;
    } else {
        cout << "ATTENTION: Solution invalide!" << endl;
    }

    return 0;
}*/


/* Fonction principale CORRIGÉE */
/*int main(int argc, char** argv) {

    // Déclarer les variables globales manquantes
    bool warm_start = true;
    bool cplex_abort = false;
    bool heuristic_emphasis = false;

    if (argc < 3) {
        cout << "Usage: " << argv[0] << " -i <fichier_entree>" << endl;
        return 1;
    }

    string fichier_entree;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            fichier_entree = argv[i + 1];
            break;
        }
    }

    if (fichier_entree.empty()) {
        cout << "Erreur: veuillez specifier un fichier d'entree avec -i" << endl;
        return 1;
    }

    // CORRECTION : appel correct de lire_graphe (sans b et g)
    lire_graphe(fichier_entree, n_sommets, voisins);

    // Initialiser b et g si nécessaire (avec des valeurs par défaut)
    // b.resize(n_sommets, 0);
    // g.resize(n_sommets, 0);

    cout << "========================================" << endl;
    cout << "Local Branching pour PIDS" << endl;
    cout << "Instance: " << fichier_entree << endl;
    cout << "Nombre de sommets: " << n_sommets << endl;
    cout << "Temps limite: " << temps_total_limit << " secondes" << endl;
    cout << "Parametres: alpha=" << alpha << ", beta=" << beta << ", k=" << k << endl;
    cout << "========================================" << endl;

    Solution meilleure_solution = algorithme_local_branching();

    cout << "\n=== RESULTATS FINAUX ===" << endl;
    cout << "Score optimal: " << meilleure_solution.score << endl;
    cout << "Nombre de sommets selectionnes: " << meilleure_solution.vertices.size() << endl;

    vector<int> sommets_tries(meilleure_solution.vertices.begin(),
                             meilleure_solution.vertices.end());
    sort(sommets_tries.begin(), sommets_tries.end());

    cout << "Sommets selectionnes: ";
    for (size_t i = 0; i < sommets_tries.size(); ++i) {
        cout << sommets_tries[i];
        if (i < sommets_tries.size() - 1) cout << " ";
    }
    cout << endl;

    // Verification de la solution
    bool solution_valide = true;
    for (int i = 0; i < n_sommets; ++i) {
        if (!est_domine(i, meilleure_solution)) {
            solution_valide = false;
            cout << "ERREUR: Sommet " << i << " non domine!" << endl;
            break;
        }
    }

    if (solution_valide) {
        cout << "Solution verifiee: tous les sommets sont domines." << endl;
    } else {
        cout << "ATTENTION: Solution invalide!" << endl;
    }

    return 0;
}*/


/* Fonction principale */
int main(int argc, char** argv) {

    // Vérifier le nombre d'arguments
    if (argc < 2) {
        afficher_aide();
        return 1;
    }

    string option_mode = "";
    string fichier_entree = "";

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            option_mode = "-i";
            if (i + 1 < argc) fichier_entree = argv[++i];
        } 
        else if (strcmp(argv[i], "-c") == 0) {
            option_mode = "-c";
            if (i + 1 < argc) fichier_entree = argv[++i];
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            temps_total_limit = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-ti") == 0 && i + 1 < argc) {
            temps_intensification_limit = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-td") == 0 && i + 1 < argc) {
            temps_diversification_limit = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            alpha = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            param_beta = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            k = atoi(argv[++i]);
        }
    }

    if (option_mode == "") {
         cout << "Erreur: veuillez specifier un mode (-i ou -c)" << endl;
         afficher_aide();
         return 1;
    }

    if (fichier_entree.empty()) {
        cout << "Erreur: veuillez specifier un fichier d'entree" << endl;
        return 1;
    }

    // Initialiser le générateur aléatoire
    srand(time(NULL));

    if (option_mode == "-c") {
        // MODE CONVERSION : matrice -> liste d'arêtes
        cout << "MODE CONVERSION" << endl;
        cout << "Conversion du fichier : " << fichier_entree << endl;

        if (convertir_matrice_vers_liste_aretes(fichier_entree)) {
            cout << "Conversion terminée avec succès !" << endl;
        } else {
            cout << "Échec de la conversion." << endl;
            return 1;
        }

        return 0;

    } else if (option_mode == "-i") {
        // MODE EXECUTION : algorithme sur liste d'arêtes
        cout << "MODE EXECUTION DE L'ALGORITHME" << endl;

        // Ouvrir le fichier de log
        string logFilename = fichier_entree + "OUTPUT_LocalBranching";
        ofstream logFile(logFilename.c_str());

        // Lire le graphe depuis le fichier liste d'arêtes
        lire_graphe(fichier_entree, n_sommets, voisins);

        cout << "========================================" << endl;
        cout << "Local Branching pour PIDS" << endl;
        cout << "Instance: " << fichier_entree << endl;
        cout << "Nombre de sommets: " << n_sommets << endl;
        cout << "Temps limite: " << temps_total_limit << " secondes" << endl;
        cout << "Parametres: alpha=" << alpha << ", beta=" << param_beta << ", k=" << k << endl;
        cout << "Temps Intensif: " << temps_intensification_limit << ", Temps Diversif: " << temps_diversification_limit << endl;
        cout << "========================================" << endl;

        if (logFile.is_open()) {
            logFile << "========================================" << endl;
            logFile << "Local Branching pour PIDS" << endl;
            logFile << "Instance: " << fichier_entree << endl;
            logFile << "Nombre de sommets: " << n_sommets << endl;
            logFile << "Temps limite: " << temps_total_limit << " secondes" << endl;
            logFile << "Parametres: alpha=" << alpha << ", beta=" << param_beta << ", k=" << k << endl;
            logFile << "Temps Intensif: " << temps_intensification_limit << ", Temps Diversif: " << temps_diversification_limit << endl;
            logFile << "========================================" << endl;
        }

        // Mesurer le temps global
        clock_t start_time = clock();

        // Exécuter l'algorithme
        Solution meilleure_solution = algorithme_local_branching(logFile);

        clock_t end_time = clock();
        double total_time = double(end_time - start_time) / CLOCKS_PER_SEC;

        // Afficher les résultats
        cout << "\n=== RESULTATS FINAUX ===" << endl;
        cout << "Score optimal: " << meilleure_solution.score << endl;
        cout << "Nombre de sommets selectionnes: " << meilleure_solution.vertices.size() << endl;
        cout << "Temps total d'execution: " << total_time << " secondes" << endl;

        if (logFile.is_open()) {
            logFile << "\n=== RESULTATS FINAUX ===" << endl;
            logFile << "Score optimal: " << meilleure_solution.score << endl;
            logFile << "Temps total d'execution: " << total_time << " secondes" << endl;
        }

        vector<int> sommets_tries(meilleure_solution.vertices.begin(),
                                 meilleure_solution.vertices.end());
        sort(sommets_tries.begin(), sommets_tries.end());

        cout << "Sommets selectionnes: ";
        if (logFile.is_open()) logFile << "Sommets selectionnes: ";
        
        for (size_t i = 0; i < sommets_tries.size(); ++i) {
            cout << sommets_tries[i];
            if (logFile.is_open()) logFile << sommets_tries[i];
            
            if (i < sommets_tries.size() - 1) {
                cout << " ";
                if (logFile.is_open()) logFile << " ";
            }
        }
        cout << endl;
        if (logFile.is_open()) logFile << endl;

        // Vérification de la solution
        bool solution_valide = true;
        for (int i = 0; i < n_sommets; ++i) {
            if (!est_domine(i, meilleure_solution)) {
                solution_valide = false;
                cout << "ERREUR: Sommet " << i << " non domine!" << endl;
                if (logFile.is_open()) logFile << "ERREUR: Sommet " << i << " non domine!" << endl;
                break;
            }
        }

        if (solution_valide) {
            cout << "Solution verifiee: tous les sommets sont domines." << endl;
            if (logFile.is_open()) logFile << "Solution verifiee: tous les sommets sont domines." << endl;
        } else {
            cout << "ATTENTION: Solution invalide!" << endl;
            if (logFile.is_open()) logFile << "ATTENTION: Solution invalide!" << endl;
        }

        if (logFile.is_open()) logFile.close();

        return 0;

    } else {
        // Option invalide
        // Should be caught by option_mode check above, but for safety:
        cout << "Erreur: mode inconnu" << endl;
        return 1;
    }
}
