#ifndef PWSOLVER_H
#define PWSOLVER_H

#include "data/problem.h"
#include "data/path.h"
#include "algorithms/dynamic_programming/PW_default/PW_default.h"
#include "algorithms/dynamic_programming/PW_acyclic/PW_acyclic.h"
#include "algorithms/dynamic_programming/PW_bucket/PW_bucket.h"
#include <thread>
#include <mutex>
#include <utility>

class Solver{

public:

    /** Solver management **/
    Solver();
    ~Solver();

    //General solver methods
    void printWelcome();
    bool getStatus(){return solver_status;}
    void setStatus(int status) {solver_status = status;}

    //Setup
    void readConfiguration(std::string file_path = "");
    void setupOutput();

    //Additional interfaces
    void setConsoleVerbosity(int verbosity);

    /** Problem management **/
    void readProblem(std::string file_name = "");                                           //Use default problem/reader
    void setCustomProblem(Problem& problem);                                                //Set a custom problem
    Problem* getProblem() {return problem;}                                                 //Get problem pointer

    // Topology getters
    int getNumberOfNodes();
    int getOrigin()       {return problem->getOrigin();}
    int getDestination()  {return problem->getDestination();}
    bool isDirected()     {return problem->isDirected();}
    bool isSymmetric()    {return problem->isSymmetric();}
    bool isGraphCyclic()  {return problem->isGraphCyclic();}

    // Problem metadata
    std::string getName() {return problem->getName();}
    int getNumRes()       {return problem->getNumRes();}

    // Objective getters
    double getNodeCost(int i)          {return problem->objExists() ? problem->getObj()->getNodeCostDouble(i) : 0.0;}
    std::vector<double> getNodeCosts() {return problem->objExists() ? problem->getObj()->getData()->getNodeCostsDouble() : std::vector<double>{};}
    double getArcCost(int i, int j)    {return problem->objExists() ? problem->getObj()->getArcCostDouble(i, j) : 0.0;}
    double getObjLB()                  {return problem->objExists() ? problem->getObj()->getLBDouble() : INFMINUSDOUBLE;}
    double getObjUB()                  {return problem->objExists() ? problem->getObj()->getUBDouble() : INFPLUSDOUBLE;}
    double getInitCost()               {return init_cost;}

    // Resource getters
    int getResLB(int res_id)                          {return problem->resExists(res_id) ? problem->getRes(res_id)->getLB() : INFMINUS;}
    int getResUB(int res_id)                          {return problem->resExists(res_id) ? problem->getRes(res_id)->getUB() : INFPLUS;}
    int getResNodeLB(int res_id, int node)            {return problem->resExists(res_id) ? problem->getRes(res_id)->getNodeLB(node) : INFMINUS;}
    int getResNodeUB(int res_id, int node)            {return problem->resExists(res_id) ? problem->getRes(res_id)->getNodeUB(node) : INFPLUS;}
    int getResArcConsumption(int res_id, int i, int j){return problem->resExists(res_id) ? problem->getRes(res_id)->getArcCost(i, j) : 0;}
    int getResNodeConsumption(int res_id, int i)      {return problem->resExists(res_id) ? problem->getRes(res_id)->getNodeCost(i) : 0;}

    /** Algorithms management **/
    void setupAlgorithms();
    Algorithm* createAlgorithm(std::string name);

    //Algorithms: setters (name)
    void setAlgorithms(std::vector<std::string> names, std::vector<bool> active = {});
    void addAlgorithm(std::string name);
    void changeAlgorithm(int id, std::string name);

    //Algorithms: setters (pointer)
    void addAlgorithm(Algorithm& algorithm);
    void changeAlgorithm(int id, Algorithm& algorithm);

    //Algorithms: getter (pointer)
    Algorithm* getAlgorithm(int id);

    //Solve Problem with selected Algorithms
    void solve();

    //Reset algorithms
    void resetAlgorithm(int id, int reset_level);
    void resetAllAlgorithms(int reset_level);

    //Clear algorithms
    void clearAlgorithms();

    // Active algorithms management
    void enableAlgorithm(int id);
    void disableAlgorithm(int id);
    void enableAllAlgorithms();
    void disableAllAlgorithms();
    bool isAlgorithmEnabled(int id);
    std::vector<int> getEnabledAlgorithms();

    /** Solution management **/
    int getNumberOfSolutions() {return solutions.size();}
    void rankSolutions(std::string criteria = "objective");
    Path* getSolution(int id);
    int getSolutionStatus(int id);
    double getSolutionObjective(int id);
    double getSolutionArcCost(int id);
    double getSolutionNodeCost(int id);
    std::vector<int> getSolutionTour(int id);
    std::string getSolutionTourAsString(int id);
    std::vector<Path> getAllSolutions(){return solutions;}
    std::vector<Path> getBestSolutions(int pool_size = 1);
    Path getBestSolution();
    void clearSolutions(){solutions.clear();}

    /** Output management **/
    void printStatus();
    void printBestSolution();
    void printAllSolutions();
    void printAlgorithmsStatus();
    void printNodeCosts();

    /** Parameters interfaces **/
    bool areAlgorithmsParallel() {return algorithms_parallel;}
    void setAlgorithmsParallel(bool algorithms_parallel) {this->algorithms_parallel = algorithms_parallel;}

    /** Problem interfaces **/
    void createProblem();
    void setProblemName(std::string name) { problem->setName(std::move(name)); }
    void initProblem()  { problem->init(); }
    void buildProblem() { problem->build(); }

    // Topology management
    int setNumNodes(int n)               {return problem->setNumNodes(n);}
    int setOrigin(int origin)            {return problem->setOrigin(origin);}
    int setDestination(int destination)  {return problem->setDestination(destination);}
    int setDirected(bool directed)       {return problem->setDirected(directed);}
    int setCyclic(bool cyclic)           {return problem->setCyclic(cyclic);}
    int setSymmetric(bool symmetric)     {return problem->setSymmetric(symmetric);}
    int addArc(int i, int j)             {return problem->addArc(i, j);}
    int setPair(int pickup, int delivery) {return problem->setPair(pickup, delivery);}

    //Objective management
    int  setInitCost(double cost)                                    {init_cost = cost; return RETURN_OK;}
    int  setNodeCost(int id, double cost)                            {return problem->setNodeCost(id, cost);}
    int  setNodeCosts(std::vector<double> costs)                     {return problem->setNodeCosts(std::move(costs));}
    int  setArcCost(int i, int j, double cost)                       {return problem->setArcCost(i, j, cost);}
    void setArcMatrixCosts(std::vector<std::vector<double>> costs)   {problem->setArcMatrixCost(std::move(costs));}

    // Resources management
    int  addResource(std::string type)                              { return problem->addResource(std::move(type)); }
    int  setResBounds(int res_id, int lb, int ub)                   { return problem->setResBounds(res_id, lb, ub); }
    int  setResNodeBound(int res_id, int node, int lb, int ub)      { return problem->setResNodeBound(res_id, node, lb, ub); }
    int  setResArcConsumption(int res_id, int i, int j, int cost)   { return problem->setResArcConsumption(res_id, i, j, cost); }
    int  setResNodeConsumption(int res_id, int i, int cost)         { return problem->setResNodeConsumption(res_id, i, cost); }

private:
    void solveAlgorithm(int algorithm_code);

    //Solver
    std::string solver_version;                         //PathWyse version number
    int solver_status;                                  //Solver status
    int optimization_round;                             //Optimization round

    //Problem
    Problem* problem;                                   //Problem
    std::string default_instance;                       //Instance name
    double init_cost;

    //Algorithms
    std::vector<Algorithm*> algorithms;
    std::vector<std::string> algorithms_names;
    std::vector<bool> algorithms_active;
    
    bool algorithms_parallel;
    std::mutex mtx;
    std::vector<std::thread> algorithms_threads;

    //Solutions
    std::vector<Path> solutions;

    //Data collection
    DataCollector collector;

};
#endif
