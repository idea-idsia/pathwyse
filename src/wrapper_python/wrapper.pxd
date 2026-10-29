from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp cimport bool

cdef extern from "solver.h":
    cdef cppclass Solver:
        Solver() except +

        # Setup
        void readProblem(string file_name)
        void setupAlgorithms()

        # Algorithm management
        void setAlgorithms(vector[string] names, vector[bool] active)
        void resetAlgorithm(int id, int reset_level)
        void resetAllAlgorithms(int reset_level)
        void enableAlgorithm(int id)
        void disableAlgorithm(int id)
        void enableAllAlgorithms()
        void disableAllAlgorithms()
        bool isAlgorithmEnabled(int id)
        vector[int] getEnabledAlgorithms()
        bool areAlgorithmsParallel()
        void setAlgorithmsParallel(bool algorithms_parallel)

        # Optimization
        void solve()

        # Problem creation
        void createProblem()
        void setProblemName(string name)
        void initProblem()
        void buildProblem()

        # Topology management
        int setNumNodes(int n)
        int setOrigin(int origin)
        int setDestination(int destination)
        int setDirected(bool directed)
        int setCyclic(bool cyclic)
        int setSymmetric(bool symmetric)
        int addArc(int i, int j)
        int setPair(int pickup, int delivery)

        # Objective
        int setInitCost(double value)
        int setNodeCost(int id, double cost)
        int setNodeCosts(vector[double] costs)
        int setArcCost(int i, int j, double cost)

        # Resources
        int  addResource(string res_type)
        int  setResBounds(int res_id, int lb, int ub)
        int  setResNodeBound(int res_id, int node, int lb, int ub)
        int  setResArcConsumption(int res_id, int i, int j, int cost)
        int  setResNodeConsumption(int res_id, int i, int cost)

        # Topology getters
        int getNumberOfNodes()
        int getOrigin()
        int getDestination()
        bool isDirected()
        bool isSymmetric()
        bool isGraphCyclic()

        # Problem metadata
        string getName()
        int getNumRes()

        # Objective getters
        double getNodeCost(int i)
        vector[double] getNodeCosts()
        double getArcCost(int i, int j)
        double getObjLB()
        double getObjUB()
        double getInitCost()

        # Resource getters
        int getResLB(int res_id)
        int getResUB(int res_id)
        int getResNodeLB(int res_id, int node)
        int getResNodeUB(int res_id, int node)
        int getResArcConsumption(int res_id, int i, int j)
        int getResNodeConsumption(int res_id, int i)

        # Solution management
        void rankSolutions(string key)
        int getNumberOfSolutions()
        int getSolutionStatus(int sol_id)
        double getSolutionObjective(int sol_id)
        double getSolutionArcCost(int sol_id)
        double getSolutionNodeCost(int sol_id)
        vector[int] getSolutionTour(int sol_id)
        void clearSolutions()

        # Output
        void printNodeCosts()
        void printBestSolution()
        void printAllSolutions()

