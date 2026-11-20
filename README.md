<div align="center">

<h1>PathWyse</h1>

![C++](https://img.shields.io/badge/C%2B%2B-20%2B-4584b6?logo=c%2B%2B&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.13%2B-4584b6?logo=python&logoColor=white)
![License](https://img.shields.io/badge/License-GPLv3-4584b6?logo=&logoColor=white)


**A C++ and Python framework to model and solve Resource Constrained Shortest Path Problems.**

</div>



---

PathWyse is a project designed to address Resource Constrained Shortest Path Problems (RCSPP), combining state-of-the-art exact algorithms with a modular architecture for rapid experimentation and integration. It is particularly suited for problems arising as pricing subproblems in column generation, as well as routing and scheduling applications with multiple constraints. 

The library can be used both as a high-performance C++ solver for real-world applications and as a research platform for developing and testing new algorithmic ideas.

Pathwyse provides an interactive mode in Python, along with Python interfaces for flexible usage.

The library is described in the paper: https://doi.org/10.1080/10556788.2023.2296978  
An earlier version is available as a technical report: https://doi.org/10.48550/arXiv.2306.08622


PathWyse is distributed under a dual licensing scheme: this repository contains the open-source version.

---

## A quick look

Once compiled, PathWyse can be used immediately as a standalone solver:

```
./bin/pathwyse instances/toy.txt
```

Example output:

```
[ Solution ]
  Solution Status                 : Optimal
  Obj                             : -10435
  Tour Length                     : 4
  Tour                            : 0 2 1 4 
  Consumption                     : 9 
```

---
## For industries

PathWyse can be used as a high-performance solver for constrained shortest path problems. It can be executed from the command line, embedded in C++ applications, or accessed through a Python interface.

PathWyse provides practitioners with fast, reliable exact solutions to complex constrained shortest path problems, helping to reduce planning time, resource utilization, and, in general, support decision-making in real-world transportation, logistic and scheduling tasks. Typical applications also include solving pricing subproblems in column generation for vehicle routing, crew rostering, path-based scheduling and many other problems.

The library includes configuration files for controlling algorithmic behavior and collecting performance data, so it can be integrated into larger pipelines without modifying the source code.

---

## For academia

PathWyse is also designed as a research-oriented framework where algorithmic components are explicit and configurable. The goal is to allow experimentation without reimplementing the full machinery behind labeling algorithms.

The library provides implementations of bidirectional dynamic programming, DSSR and NG-path relaxations, and their hybridizations, together with mechanisms for label candidate selection, dominance, and join procedures.

Its internal structure is modular: resources, labels, and extension functions can be customized, and new algorithmic variants can be introduced without modifying the entire codebase. This makes the library suitable for studying new problems, designing dominance rules and relaxation schemes, or formulating pricing subproblems in column generation.

---

## Supported problems

PathWyse focuses on resource constrained shortest path problems with one or more monotone resources. It supports acyclic networks and includes specialized techniques for elementary variants on cyclic networks through relaxation schemes. 

The current version includes support for common resources such as capacity, time, node limits, and time windows. These can be combined to model a variety of problems, including routing and scheduling with cumulative constraints, and pricing subproblems in column generation.


The framework is fully extensible: new types of resources can be implemented by extending the Resource class, making it possible to model custom constraints for a wide range of practical and research problems.

---

## Main techniques

PathWyse includes the following techniques and features:

- **Bi-directional search:** Dynamic programming labeling algorithms are implemented in both mono-directional and bi-directional [Righini and Salani, 2006] fashion;
- **Relaxation schemes:** Decremental State Space Relaxation (DSSR) [Righini and Salani, 2008], NG-path relaxations (NG) [Baldacci et al., 2011] and hybridizations [Martinelli et al., 2014] are included to tackle the Elementary RCSPP;
- **Dynamic half-way point:** The budget of the critical resource is automatically adjusted in bidirectional algorithms, similarly to [Sadykov et al., 2020]
- **Candidate and join strategies:** multiple methods for selecting candidates and performing join operations in labeling algorithms.
- **A\*-based algorithm:** a custom implementation of the A* algorithm proposed by [Ahmadi et al. 2021] is provided for single resource acyclic problems;

PathWyse also supports simple heuristics, bucket-based label organization (PWBucket), and features data collection tools.

---

## Getting started

To compile the project, follow the instructions in: [Installation](https://idea.idsia.ch/pathwyse/documentation/INSTALL/)

After compilation, instances can be solved from the command line:

```
./bin/pathwyse path-to-instance
```

The instance format is described in: [Instance Format](https://idea.idsia.ch/pathwyse/documentation/INSTANCE/)

We provide a toy instance under `instances/toy.txt` to verify the installation. Additional benchmark instances from the literature are available under `instances/`:

- `instances/spprclib/` — SPPRCLIB [Jepsen et al., 2008]
- `instances/dimacs/` — DIMACS 9th Challenge
- `instances/longestpath/` — LongestPath [Salani et al., 2024]

The file `pathwyse.set` contains the main configuration parameters. 
A complete description of the available options is provided in: [Parameters](https://idea.idsia.ch/pathwyse/documentation/PARAMETERS_TABLE/)
Recommended configurations for cyclic and acyclic networks are provided in: [Suggested Settings](https://idea.idsia.ch/pathwyse/documentation/SUGGESTED_SETTINGS/)

Command-line keywords for overriding instance data at runtime are described in: [Extras](https://idea.idsia.ch/pathwyse/documentation/EXTRAS/).

Instructions for using PathWyse from Python are available in: [Python](https://idea.idsia.ch/pathwyse/python/PYTHON/)

---

## Documentation

The full documentation, including installation guides, API references, and tutorials, is available at: https://idea.idsia.ch/pathwyse/

The Solver class provides the main interface for interacting with the library programmatically.

---

## License

PathWyse is distributed under a dual licensing scheme.

This repository is open source and released under the GNU General Public License v3.0.

The Enterprise version includes additional and experimental features, as well as extended support. It is available for academic collaborations and under a commercial license for non-open source projects. 

For further information, contact: pathwyse@supsi.ch

---

## How to cite

Salani, M., Basso, S., & Giuffrida, V. (2024).  
PathWyse: a flexible, open-source library for the resource constrained shortest path problem.  
Optimization Methods and Software. https://doi.org/10.1080/10556788.2023.2296978