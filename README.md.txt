# PIDS Solvers: Methods and Execution Guide

This repository contains the implementation of two distinct methods for solving the Positive Influence Dominating Set (PIDS) problem. 

## 1. CPLEX Exact Model (`lunchModelCplex.cpp`)

### How it works:
This is an **exact method**. It formulates the PIDS problem mathematically as an Integer Linear Programming (ILP) model. To solve this model, it uses the commercial solver **IBM ILOG CPLEX** to find the absolute optimal solution (the minimum possible cardinality) with a proven mathematical guarantee. This method is highly accurate but may take a long time to finish on very large or complex graphs.

### Compilation:
*(Assuming your environment is configured with CPLEX libraries)*
```bash
g++ -O3 lunchModelCplex.cpp -o lunchModelCplex
```

### Execution:
To run the exact solver on a single instance file (where the time limit is strictly respected):
```bash
./lunchModelCplex <filename> <time_limit_in_seconds>
```
**Example:**
```bash
./lunchModelCplex instances/Grafo30.txt 3600
```
*This will generate temporary `.lp` (linear programming format) and `.script` files, launch CPLEX, and output the optimal Cardinality and the objective score.*

---

## 2. Local Branching Meta-Heuristic (`localBranching.cpp`)

### How it works:
This is an **approximate method (meta-heuristic)**. Instead of trying to prove optimality mathematically like the exact model, it starts with an initial valid solution and iteratively attempts to improve it. It uses CPLEX internally to explore small "neighborhoods" around the current solution (intensification) and makes larger random structural changes when it gets stuck (diversification) to escape local optima. This method is usually much faster on large graphs and provides extremely high-quality solutions, even if they aren't guaranteed to be the absolute minimum.

### Compilation:
```bash
g++ -O3 localBranching.cpp -o lb_pids
```

### Execution:
To run the algorithm on an instance (using an edge list file):
```bash
./lb_pids -i <edge_list_file.txt> [options]
```

**Main Parameters:**
- `-t <double>` : Total time limit in seconds (default: 100.0)
- `-ti <double>` : Intensification time limit in seconds (default: 10.0)
- `-td <double>` : Diversification time limit in seconds (default: 10.0)
- `-a <double>` : Alpha (perturbation percentage for shaking up the solution) (default: 0.3)
- `-b <double>` : Beta (destruction percentage) (default: 0.4)
- `-k <int>`    : K (Hamming Distance allowed during local search) (default: 2)

**Example:**
```bash
./lb_pids -i instances/Grafo30.txt -t 600 -ti 10 -td 10 -a 0.05 -b 0.4 -k 20
```
