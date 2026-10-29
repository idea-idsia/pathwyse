from libcpp.vector cimport vector
from libcpp.string cimport string
from libcpp cimport bool

# Status (for setters operations)
PW_OK   = 1
PW_FAIL = 0

cdef class PWSolver:
    cdef Solver* s

    def __cinit__(self):
        self.s = new Solver()

    def __dealloc__(self):
        del self.s

    # --- Setup ---

    def readProblem(self, file_name):
        self.s.readProblem(file_name.encode('utf-8'))

    def setupAlgorithms(self):
        self.s.setupAlgorithms()

    # --- Algorithm management ---

    def setAlgorithms(self, names, active=None):
        cdef vector[string] cnames = [n.encode('utf-8') for n in names]
        cdef vector[bool] cactive
        if active is None:
            active = []
        for a in active:
            cactive.push_back(a)
        self.s.setAlgorithms(cnames, cactive)

    def resetAlgorithm(self, int id, int reset_level):
        self.s.resetAlgorithm(id, reset_level)

    def resetAllAlgorithms(self, int reset_level):
        self.s.resetAllAlgorithms(reset_level)

    def enableAlgorithm(self, int id):
        self.s.enableAlgorithm(id)

    def disableAlgorithm(self, int id):
        self.s.disableAlgorithm(id)

    def enableAllAlgorithms(self):
        self.s.enableAllAlgorithms()

    def disableAllAlgorithms(self):
        self.s.disableAllAlgorithms()

    def isAlgorithmEnabled(self, int id):
        return self.s.isAlgorithmEnabled(id)

    def getEnabledAlgorithms(self):
        cdef vector[int] result = self.s.getEnabledAlgorithms()
        return list(result)

    def areAlgorithmsParallel(self):
        return self.s.areAlgorithmsParallel()

    def setAlgorithmsParallel(self, bool parallel):
        self.s.setAlgorithmsParallel(parallel)

    # --- Optimization ---

    def solve(self):
        self.s.solve()

    # --- Problem creation ---

    def createProblem(self):
        self.s.createProblem()

    def setProblemName(self, name):
        self.s.setProblemName(name.encode('utf-8'))

    def initProblem(self):
        self.s.initProblem()

    def buildProblem(self):
        self.s.buildProblem()

    # --- Topology management ---

    def setNumNodes(self, int n):
        return self.s.setNumNodes(n)

    def setOrigin(self, int origin):
        return self.s.setOrigin(origin)

    def setDestination(self, int destination):
        return self.s.setDestination(destination)

    def setDirected(self, bool directed):
        return self.s.setDirected(directed)

    def setCyclic(self, bool cyclic):
        return self.s.setCyclic(cyclic)

    def setSymmetric(self, bool symmetric):
        return self.s.setSymmetric(symmetric)

    def addArc(self, int i, int j):
        return self.s.addArc(i, j)

    def setPair(self, int pickup, int delivery):
        return self.s.setPair(pickup, delivery)

    # --- Objective ---

    def setInitCost(self, double value):
        return self.s.setInitCost(value)

    def setNodeCost(self, int id, double cost):
        return self.s.setNodeCost(id, cost)

    def setNodeCosts(self, costs):
        cdef vector[double] c_costs = costs
        return self.s.setNodeCosts(c_costs)

    def setArcCost(self, int i, int j, double cost):
        return self.s.setArcCost(i, j, cost)

    # --- Resources ---

    def addResource(self, res_type):
        return self.s.addResource(res_type.encode('utf-8'))

    def setResBounds(self, int res_id, int lb, int ub):
        return self.s.setResBounds(res_id, lb, ub)

    def setResNodeBound(self, int res_id, int node, int lb, int ub):
        return self.s.setResNodeBound(res_id, node, lb, ub)

    def setResArcConsumption(self, int res_id, int i, int j, int cost):
        return self.s.setResArcConsumption(res_id, i, j, cost)

    def setResNodeConsumption(self, int res_id, int i, int cost):
        return self.s.setResNodeConsumption(res_id, i, cost)

    # --- Getters ---

    # Topology
    def getNumberOfNodes(self):
        return self.s.getNumberOfNodes()

    def getOrigin(self):
        return self.s.getOrigin()

    def getDestination(self):
        return self.s.getDestination()

    def isDirected(self):
        return self.s.isDirected()

    def isSymmetric(self):
        return self.s.isSymmetric()

    def isGraphCyclic(self):
        return self.s.isGraphCyclic()

    # Problem metadata
    def getName(self):
        return self.s.getName().decode('utf-8')

    def getNumRes(self):
        return self.s.getNumRes()

    # Objective
    def getNodeCost(self, int i):
        return self.s.getNodeCost(i)

    def getNodeCosts(self):
        cdef vector[double] c_costs = self.s.getNodeCosts()
        return list(c_costs)

    def getArcCost(self, int i, int j):
        return self.s.getArcCost(i, j)

    def getObjLB(self):
        return self.s.getObjLB()

    def getObjUB(self):
        return self.s.getObjUB()

    def getInitCost(self):
        return self.s.getInitCost()

    # Resources
    def getResLB(self, int res_id):
        return self.s.getResLB(res_id)

    def getResUB(self, int res_id):
        return self.s.getResUB(res_id)

    def getResNodeLB(self, int res_id, int node):
        return self.s.getResNodeLB(res_id, node)

    def getResNodeUB(self, int res_id, int node):
        return self.s.getResNodeUB(res_id, node)

    def getResArcConsumption(self, int res_id, int i, int j):
        return self.s.getResArcConsumption(res_id, i, j)

    def getResNodeConsumption(self, int res_id, int i):
        return self.s.getResNodeConsumption(res_id, i)

    # --- Solution management ---

    def rankSolutions(self, criteria):
        self.s.rankSolutions(criteria.encode('utf-8'))

    def getNumberOfSolutions(self):
        return self.s.getNumberOfSolutions()

    def getSolutionStatus(self, int sol_id):
        return self.s.getSolutionStatus(sol_id)

    def getSolutionObjective(self, int sol_id):
        return self.s.getSolutionObjective(sol_id)

    def getSolutionArcCost(self, int sol_id):
        return self.s.getSolutionArcCost(sol_id)

    def getSolutionNodeCost(self, int sol_id):
        return self.s.getSolutionNodeCost(sol_id)

    def getSolutionTour(self, int sol_id):
        cdef vector[int] vtour = self.s.getSolutionTour(sol_id)
        return list(vtour)

    def clearSolutions(self):
        self.s.clearSolutions()

    # --- Output ---

    def printNodeCosts(self):
        self.s.printNodeCosts()

    def printBestSolution(self):
        self.s.printBestSolution()

    def printAllSolutions(self):
        self.s.printAllSolutions()

