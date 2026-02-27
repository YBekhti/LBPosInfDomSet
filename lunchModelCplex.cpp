#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <chrono>

using namespace std;
using namespace std::chrono;

// CPLEX Executable Path
string getCplexPath() {
    const char* env_p = getenv("CPLEX_BIN");
    if (env_p) return string(env_p);
    
    #ifdef _WIN32
        return "\"C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio128\\cplex\\bin\\x64_win64\\cplex.exe\"";
    #else
        return "cplex"; // Default fallback
    #endif
}

const string CPLEX_PATH = getCplexPath();

// Function to read the instance
vector<vector<int>> readInstance(const string& filename, int& n, int& m) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        exit(1);
    }

    if (!(file >> n)) {
        cerr << "Error reading n" << endl;
        exit(1);
    }
    if (!(file >> m)) {
        cerr << "Error reading m" << endl;
        exit(1);
    }
    
    cout << "Instance info: n=" << n << ", m=" << m << endl;

    vector<vector<int>> adj(n, vector<int>(n));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!(file >> adj[i][j])) {
                cerr << "Error reading matrix at " << i << "," << j << endl;
                exit(1);
            }
        }
    }
    return adj;
}

int main(int argc, char** argv) {
   // remove("cplex.log"); // Clear previous log
    string filename;
    double time_limit = 3600.0; // Default to 1 hour

    if (argc > 1) {
        filename = argv[1];
    } else {
        filename = "instances/Grafo50.txt"; // Default fallback
        cout << "No file specified, defaulting to " << filename << endl;
    }

    if (argc > 2) {
        time_limit = atof(argv[2]);
    }
    
    int n, m;
    
    cout << "Processing " << filename << "..." << endl;
    vector<vector<int>> a = readInstance(filename, n, m);

    // Use unique filenames based on instance name to avoid race conditions
    string baseName = filename;
    size_t lastSlash = baseName.find_last_of("/\\");
    if (lastSlash != string::npos) baseName = baseName.substr(lastSlash + 1);
    
    string lpFilename = baseName + ".lp";
    string scriptFilename = baseName + ".script";
    string logFilename = baseName + ".log";

    // 1. Generate LP File
    ofstream lpFile(lpFilename.c_str());
    if (!lpFile.is_open()) {
        cerr << "Error creating LP file: " << lpFilename << endl;
        return 1;
    }

    cout << "Generating LP file: " << lpFilename << "..." << endl;

    // LP Header
    lpFile << "Minimize" << endl;
    lpFile << " obj: ";
    for (int i = 0; i < n; i++) {
        lpFile << "x" << (i + 1);
        if (i < n - 1) lpFile << " + ";
    }
    lpFile << endl;

    lpFile << "Subject To" << endl;

    float rho = 0.5;

    for (int i = 0; i < n; i++) {
        int deg_i = 0;
        vector<int> neighbors;
        
        for (int j = 0; j < n; j++) {
            if (a[i][j] == 1) {
                deg_i++;
                neighbors.push_back(j);
            }
        }

        // Constraint: sum(x_j) + M*x_i >= ceil(0.5 * deg_i)
        int rhs = (int)ceil(rho * deg_i);

        lpFile << " c" << (i + 1) << ": ";
        
        for (size_t k = 0; k < neighbors.size(); k++) {
            lpFile << "x" << (neighbors[k] + 1) << " + ";
        }
        
        // Use rhs as the coefficient for x[i] (Big-M)
        lpFile << rhs << " x" << (i + 1);
        
        lpFile << " >= " << rhs << endl; 
    }

    lpFile << "Binaries" << endl;
    for (int i = 0; i < n; i++) {
        lpFile << " x" << (i + 1) << endl;
    }

    lpFile << "End" << endl;
    lpFile.close();
    cout << "LP file generated." << endl;

    // 2. Create Unique CPLEX Script
    ofstream scriptFile(scriptFilename.c_str());
    if (!scriptFile.is_open()) {
         cerr << "Error creating script file: " << scriptFilename << endl;
         return 1;
    }
    scriptFile << "read " << lpFilename << endl;
    scriptFile << "set timelimit " << time_limit << endl;
    scriptFile << "optimize" << endl;
    // scriptFile << "set mip display 2" << endl; // detailed log if needed
    scriptFile << "display solution variables -" << endl; // Display all non-zero variables
    scriptFile << "quit" << endl;
    scriptFile.close();

    // 3. Call CPLEX with unique script and log
    cout << "Running CPLEX..." << endl;
    // Command: cplex -f unique.script > unique.log 2>&1
    string command = CPLEX_PATH + " -f " + scriptFilename + " > " + logFilename + " 2>&1"; 
    
    auto start = high_resolution_clock::now();
    int ret = system(command.c_str());
    auto stop = high_resolution_clock::now();
    
    auto duration = duration_cast<milliseconds>(stop - start);
    double seconds = duration.count() / 1000.0;
    
    cout << "Execution Time: " << seconds << " s" << endl;

    if (ret != 0) {
        cerr << "Error running CPLEX. Return Code: " << ret << endl;
        cerr << "Log check: " << logFilename << endl;
    }

    // 4. Parse Log
    ifstream logFile(logFilename.c_str());
    string line;
    string objective = "N/A";
    string gap = "0.00%"; 
    vector<string> solutionLines;
    bool readingSolution = false;
    int cardinality = 0;

    if (logFile.is_open()) {
        while (getline(logFile, line)) {
            // --- Objective parsing ---
            if (line.find("Objective") != string::npos && line.find("=") != string::npos) {
                if (line.find("IInf") != string::npos || line.find("Variable") != string::npos) continue;
                
                size_t eqPos = line.find('=', line.find("Objective"));
                if (eqPos != string::npos) {
                    string val = line.substr(eqPos + 1);
                    size_t first = val.find_first_not_of(" \t");
                    if (first != string::npos) val = val.substr(first);
                    size_t end = val.find_first_of(",) \t\r\n");
                    if (end != string::npos) val = val.substr(0, end);
                    if (!val.empty()) objective = val;
                }
            }
            
            // --- Gap parsing ---
            if (line.find("gap =") != string::npos) {
                size_t percentPos = line.find('%');
                if (percentPos != string::npos) {
                    size_t numStart = percentPos;
                    while (numStart > 0 && (isdigit(line[numStart-1]) || line[numStart-1] == '.')) {
                        numStart--;
                    }
                    if (numStart < percentPos) {
                        gap = line.substr(numStart, percentPos - numStart + 1);
                    }
                } else {
                    size_t pos = line.find("gap =");
                    string raw = line.substr(pos + 5);
                    size_t first = raw.find_first_not_of(" \t");
                    if (first != string::npos) raw = raw.substr(first);
                    size_t end = raw.find_first_of(",) \t");
                    if (end != string::npos) raw = raw.substr(0, end);
                    gap = raw;
                }
            }
            
            // --- Solution Section ---
            if (line.find("Variable Name") != string::npos && line.find("Solution Value") != string::npos) {
                readingSolution = true;
                continue;
            }
            if (readingSolution) {
                if (line.find("All other variables") != string::npos || line.find("CPLEX>") != string::npos) {
                    readingSolution = false;
                } else if (!line.empty()) {
                    solutionLines.push_back(line);
                    if (line.find("1.000000") != string::npos) {
                        cardinality++;
                    }
                }
            }
        }
        logFile.close();
    } else {
        cerr << "Could not open " << logFilename << " for parsing." << endl;
    }

    // Output results
    cout << "\n--- Final Results ---" << endl;
    cout << "Objective: " << objective << endl;
    cout << "Gap: " << gap << endl;
    cout << "Cardinality: " << cardinality << endl;
    cout << "Solution: " << endl;
    for (size_t i = 0; i < solutionLines.size(); ++i) {
        cout << solutionLines[i] << endl;
    }

    string outputFilename = filename + "OUTPUT";
    ofstream outFile(outputFilename.c_str());
    if (outFile.is_open()) {
        outFile << "Objective: " << objective << endl;
        outFile << "Gap: " << gap << endl;
        outFile << "Cardinality: " << cardinality << endl;
        outFile << "Solution: " << endl;
        for (size_t i = 0; i < solutionLines.size(); ++i) {
            outFile << solutionLines[i] << endl;
        }
        outFile.close();
        cout << "Results saved to " << outputFilename << endl;
    } else {
        cerr << "Error saving results to " << outputFilename << endl;
    }
    
    // Cleanup temporary files
    // remove(lpFilename.c_str());
    // remove(scriptFilename.c_str());
    // remove(logFilename.c_str());

    return 0;
}
