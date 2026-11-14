#include "solver.h"
#include <filesystem>
#include "utils/logger.h"

// Solver Management

/**
 * Solver Constructor. Builds the solver.
 */
Solver::Solver() {
    setStatus(SOLVER_START);
    solver_version = "1.1";
    optimization_round = 0;
    init_cost = 0;
    problem = new Problem();
    readConfiguration();
    setupOutput();
    printWelcome();
}

/**
 * Solver Deconstructor. Destroys the solver.
 */
Solver::~Solver() {
    delete problem;
    clearAlgorithms();
}

// General solver methods

/**
 * Prints welcome message and version to console.
 */
void Solver::printWelcome() {
    if(Parameters::getVerbosity() < 0) return;

    Logger::log(("[ PathWyse ver. " + solver_version + "]"), VERB_STD, BOLD);
    Logger::divider();
}

/**
 * Reads and setups settings from configuration file.
 * 
 * @param file_path - Configuration file path.
 */
void Solver::readConfiguration(std::string file_path){
    Parameters::readParameters(file_path);
    default_instance = Parameters::getInstancePath();
    algorithms_names = Parameters::getAlgoNames();
    algorithms_active = Parameters::getAlgoActive();
    algorithms_parallel = Parameters::areAlgoParallel();
}

/**
 * Setups output folders for data collection.
 */
void Solver::setupOutput() {
    //Setup solution collector
    collector = DataCollector("Solutions");
    std::cout<<std::fixed;

    if(not Parameters::isOutputStored())
        return;

    //Setup output folders
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now), "%F-%T");
    if (oss.str().length() <= 1) {
        oss.str("");
        oss.clear();
        oss << std::put_time(std::localtime(&now), "%Y-%m-%d_%H-%M-%S");
    }
    Parameters::setTimestamp(oss.str());

    //Creates folders for the experiments
    Parameters::setupCollectionPath();
    std::string data_collection_path = Parameters::getCollectionPath();

    if(not data_collection_path.empty())
        std::filesystem::create_directories(data_collection_path);

    //Store set file for experiment
    if(std::filesystem::exists("pathwyse.set"))
        std::filesystem::copy("pathwyse.set", data_collection_path + "pathwyse.set");
}

/**
 * Sets console verbosity.
 * 
 * @param verbosity - Verbosity level.
 */
void Solver::setConsoleVerbosity(int verbosity) {
    Parameters::setVerbosity(verbosity);
}

// Problem management

/**
 * Reads problem instance from file.
 * 
 * @param file_name - Name of the problem file.
 */
void Solver::readProblem(std::string file_name) {
    problem = new Problem();

    if(file_name.empty())
        file_name = default_instance;

    problem->readProblem(file_name);
    setStatus(SOLVER_READY);
}

/**
 * Creates a problem instance.
 */
void Solver::createProblem() {
    delete problem;
    problem = new Problem();
    setStatus(SOLVER_READY);
}

/**
 * Sets already definied custom problem
 * 
 * @param problem - Problem object (pointer).
 */
void Solver::setCustomProblem(Problem& problem) {
    this->problem = &problem;
    setStatus(SOLVER_READY);
}

/**
 * Gets the number of nodes in the network
 * 
 * @return int - Number of nodes in the network.
 */
int Solver::getNumberOfNodes() {
    return problem ? problem->getNumNodes() : 0;
}

// Algorithms management

/**
 * Setups algorithms.
 */
void Solver::setupAlgorithms() {
    if (problem)
        setAlgorithms(algorithms_names, algorithms_active);
    else
        Logger::error("No problem defined");
}

/**
 * Creates algorithm object.
 * 
 * @param name - Algorithm name.
 * @return Algorithm* - New algorithm pointer.
 */
Algorithm* Solver::createAlgorithm(std::string name){
    if(name == "PWAcyclic")
        return new PWAcyclic(name, problem);
    else if (name == "PWBucket" or name == "PWBucketRelaxDom")
        return new PWBucket(name, problem);
    else
        return new PWDefault(name, problem);
}

// Algorithms: setters (name)

/**
 * Sets new pool of algorithms. Clears the current pool.
 * 
 * @param names - Names of algorithms to add to the pool.
 * @param active - Initial state of the added algorithms.
 */
void Solver::setAlgorithms(std::vector<std::string> names, std::vector<bool> active) {
     //Reset Data Structure
     clearAlgorithms();
    for(int i = 0; i < names.size(); i++) {
        algorithms.push_back(createAlgorithm(names[i]));
        algorithms_active.push_back(i < (int)active.size() ? active[i] : true);
    }
}

/**
 * Adds one algorithm object to the pool. Starts active.
 * 
 * @param name - Name of the algorithm to add to the pool.
 */
void Solver::addAlgorithm(std::string name) {
    algorithms.push_back(createAlgorithm(name));
    algorithms_active.push_back(true);
}

/**
 * Changes one algorithm in the pool. New algorithm starts active.
 * 
 * @param id - Position in the pool of the algorithm to change.
 * @param name - Name of the new algorithm.
 */
void Solver::changeAlgorithm(int id, std::string name) {
    if(id < algorithms.size()) {
        delete algorithms[id];
        algorithms[id] = createAlgorithm(name);
        algorithms_active[id] = true;
    }
}

// Algorithm: setters (pointer)

/**
 * Adds an already defined algorithm to the pool. Starts active.
 * 
 * @param algorithm - Algorithm object (reference) to add to the pool.
 */
void Solver::addAlgorithm(Algorithm& algorithm) {
    algorithms.push_back(&algorithm);
    algorithms_active.push_back(true);
}

/**
 * Changes algorithm in the pool. New algorithm starts active.
 * 
 * @param id - Position in the pool of the algorithm to change.
 * @param algorithm - New algorithm.
 */
void Solver::changeAlgorithm(int id, Algorithm& algorithm) {
    if(id < algorithms.size()) {
        delete algorithms[id];
        algorithms[id] = &algorithm;
        algorithms_active[id] = true;
    }
}

//Algorithms: getter (pointer)

/**
 * Gets pointer of one algorithm in the pool.
 * 
 * @param id - Position of the algorithm in the pool.
 * @return Algorithm* - Algorithm from the pool (pointer).
 */
Algorithm* Solver::getAlgorithm(int id) {
    return id < algorithms.size() ? algorithms[id] : nullptr;
}

/**
 * Solves Problem with active algorithms.
 */
void Solver::solve() {
    if(not problem or not problem->validate()) {
        Logger::error("Problem is not ready, aborting solve.");
        return;
    }

    auto active = getEnabledAlgorithms();
    if(active.empty()) {
        Logger::warn("No active algorithms, skipping solve.");
        return;
    }

    optimization_round++;
    problem->scaleCosts();
    Logger::log("Solving problem...");
    setStatus(SOLVER_BUSY);

    if(algorithms_parallel) {
        algorithms_threads.clear();
        for(int id : active)
            algorithms_threads.emplace_back(&Solver::solveAlgorithm, this, id);
        for(auto& t : algorithms_threads)
            t.join();
    }
    else {
        for(int id : active)
            solveAlgorithm(id);
    }

    Logger::log("Solving complete");
    Logger::divider();
    Logger::log("[ Optimization ]", VERB_STD, BOLD);
    Logger::log("Active algorithms", std::to_string(active.size()));
    Logger::log("Parallel mode", std::to_string(algorithms_parallel));
    Logger::log("Solutions count", std::to_string(solutions.size()));
    printStatus();
    setStatus(SOLVER_READY);
}

/**
 * Solves the problem with a specified algorithm. Private method used by solve().
 * 
 * @param id - Identifier of the algorithm to solve.
 */
void Solver::solveAlgorithm(int id) {
    if(id >= algorithms.size()) return;

    Algorithm* algorithm = algorithms[id];
    algorithm->setExecutionID(optimization_round);
    algorithm->solve();
    std::vector<Path> algorithm_solutions = algorithm->getSolutions();
    if(algorithm_solutions.empty()) return;

    if(algorithms_parallel) mtx.lock();
    solutions.insert(solutions.end(), algorithm_solutions.begin(), algorithm_solutions.end());
    if(algorithms_parallel) mtx.unlock();
}

// Reset algorithms

/**
 * Resets algorithm in the pool.
 * 
 * @param id - Position of the algorithm to reset.
 * @param reset_level - Reset level.
 */
void Solver::resetAlgorithm(int id, int reset_level) {
    if(id < algorithms.size() and algorithms[id])
        algorithms[id]->resetAlgorithm(reset_level);
}

/**
 * Resets all algorithms in the pool.
 * 
 * @param reset_level - Reset level.
 */
void Solver::resetAllAlgorithms(int reset_level) {
    for(auto & a : algorithms)
        a->resetAlgorithm(reset_level);
}

// Clear algorithms

/**
 * Clears all algorithms in the pool.
 */
void Solver::clearAlgorithms() {
    for(auto* a : algorithms)
        delete a;
    algorithms.clear();
    algorithms_active.clear();
}

// Active algorithms management

void Solver::enableAlgorithm(int id) {
    if(id < algorithms_active.size()) algorithms_active[id] = true;
}

void Solver::disableAlgorithm(int id) {
    if(id < algorithms_active.size()) algorithms_active[id] = false;
}

void Solver::enableAllAlgorithms() {
    std::fill(algorithms_active.begin(), algorithms_active.end(), true);
}

void Solver::disableAllAlgorithms() {
    std::fill(algorithms_active.begin(), algorithms_active.end(), false);
}

bool Solver::isAlgorithmEnabled(int id) {
    return id < algorithms_active.size() and algorithms_active[id];
}

std::vector<int> Solver::getEnabledAlgorithms() {
    std::vector<int> active;
    for(int i = 0; i < algorithms_active.size(); i++)
        if(algorithms_active[i]) active.push_back(i);
    return active;
}

/** Solution Management **/

/**
 * Orders solutions by a given criteria. By default they are ordered by objective function value.
 * 
 * @param criteria - Ranking criteria. Objective is the only criteria supported at this stage.
 */
void Solver::rankSolutions(std::string criteria){
    std::sort(solutions.begin(), solutions.end(), less_than_objective_status());
}

/**
 * Gets solution.
 * 
 * @param id - Id of the solution to get.
 * @return Path* - Solution Path object (pointer).
 */
Path* Solver::getSolution(int id) {
    return id < solutions.size() ? & solutions[id] : nullptr;
}

/**
 * Gets solution status.
 * 
 * @param id - Id of the solution from which retrieve the status.
 * @return int - Solution status.
 */
int Solver::getSolutionStatus(int id) {
    return id < solutions.size() ? solutions[id].getStatus() : PATH_UNKNOWN;
}

/**
 * Gets solution objective.
 * 
 * @param id - Id of the solution from which retrieve the objective.
 * @return double - Solution objective.
 */
double Solver::getSolutionObjective(int id){
    if (id >= (int)solutions.size()) return MAX_DOUBLE;
    return init_cost + (solutions[id].getObjective() / problem->getCostScaleFactor());
}

/**
 * Gets solution arc cost
 * 
 * @param id - Id of the solution from which retrieve Arc Cost.
 * @return double - Solution arc cost.
 */
double Solver::getSolutionArcCost(int id) {
    return id < solutions.size() ? solutions[id].getArcCost()/problem->getCostScaleFactor() : MAX_DOUBLE;
}

/**
 * Gets solution node cost.
 * 
 * @param id - Id of the solution from which retrieve node cost.
 * @return double - Solution node cost.
 */
double Solver::getSolutionNodeCost(int id) {
    return id < solutions.size() ? solutions[id].getNodeCost()/problem->getCostScaleFactor() : MAX_DOUBLE;
}

/**
 * Gets tour of a solution.
 * 
 * @param id - Position of the solution from which the tour is retrieved.
 * @return std::vector<int> - Solution tour.
 */
std::vector<int> Solver::getSolutionTour(int id){
    if(id >= solutions.size())
        return {};

    auto tour_list = getSolution(id)->getTour();
    std::vector<int> tour_vec(tour_list.begin(), tour_list.end());
    return tour_vec;
}

/**
 * Gets tour of a solution, as a string.
 * 
 * @param id - Position of the solution from which retrieve solution tour as string.
 * @return std::string - Solution tour as string.
 */
std::string Solver::getSolutionTourAsString(int id) {
    return id < solutions.size() ? solutions[id].getTourAsString() : "";
}

/**
 * Returns a pool of the best solutions found.
 * 
 * @param pool_size - Number of top solutions to return.
 * @return std::vector<Path> - Vector of solutions.
 */
std::vector<Path> Solver::getBestSolutions(int pool_size){
    rankSolutions(RANK_OBJECTIVE);

    if(pool_size >= solutions.size())
        return solutions;

    std::vector<Path> top_solutions (pool_size);
    std::copy_n(solutions.begin(), pool_size, top_solutions.begin());
    return top_solutions;
}

/**
 * Returns the best solution found.
 * 
 * @return Path - Best solution found.
 */
Path Solver::getBestSolution(){
    if(solutions.empty())
        return Path();

    rankSolutions(RANK_OBJECTIVE);
    return solutions[0];
}

/** Output management **/

/**
 * Prints problem and algorithm status.
 */
void Solver::printStatus() {
    if(Parameters::getVerbosity() < 0) return;
    problem->printStatus();

    for(int i = 0; i < algorithms.size(); i++) {
        if (algorithms_active[i])
            algorithms[i]->printAlgorithm();
    }

    Logger::divider();
    for(auto & a : algorithms)
        a->printCollection();
}

/**
 * Prints best solution found.
 */
void Solver::printBestSolution() {
    if(solutions.size() > 0) getBestSolution().printPath(init_cost, problem->getCostScaleFactor());
    else Logger::warn("No solution found");
}

void Solver::printAllSolutions() {
    if(solutions.size() > 0) for (auto &s : solutions) s.printPath(init_cost, problem->getCostScaleFactor());
    else Logger::warn("No solution found");
}

/**
 * Prints pool status.
 */
void Solver::printAlgorithmsStatus(){
    for(auto & algo : algorithms)
        algo->printStatus();
}

/**
 * Prints node costs.
 */
void Solver::printNodeCosts() {
    for(int i = 0; i < problem->getNumNodes(); i++)
        std::cout<<i<<") "<<problem->getObj()->getNodeCost(i) << " ";
    std::cout<<std::endl;
}
