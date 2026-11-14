#include "algorithms/preprocessing/preprocessing.h"
#include "algorithms/preprocessing/dijkstra.h"

/** Algorithm management **/
Preprocessing::Preprocessing(std::string name, Problem* problem): Algorithm(name, problem) {
    problem->initBoundLabels();
    initDataCollection();
    dijkstra = new Dijkstra(name, problem);
    preprocessingCritical = Parameters::isPreprocessingCritical();
    preprocessingIntensity = Parameters::getPreprocessingIntensity();
    if(preprocessingCritical)
        criticalActive = Bitset(problem->getNumNodes());
    round = 0;

    setStatus(ALGO_READY);
}

//Performs preprocessing runs to computer lower and upper bunds and completion labels
void Preprocessing::solve() {
    setStatus(ALGO_OPTIMIZING);

    //Round 1B: Resource, Backward
    bool preprocess = solveRound(false, RES_CRITICAL);

    //Round 1F: Resource, Forward
    if(preprocess)
        preprocess = solveRound(true, RES_CRITICAL);

    //Round 2B: Cost, Backward
    if(preprocess)
        preprocess = solveRound(false, RES_COST);

    //Round 2F: Cost, Forward
    if(preprocess)
        preprocess = solveRound(true, RES_COST);

    //Extra Rounds: Other resources
    if(preprocessingIntensity == PREPROCESSING_FULL)
        for(int id = 1; id < problem->getNumRes(); id++){
            if(not preprocess)
                break;
            preprocess = solveRound(false, id);
            if(not preprocess)
                break;
            preprocess = solveRound(true, id);
        }

    setStatus(ALGO_DONE);
    collector.writeData();
}

//Solve a Dijkstra round
bool Preprocessing::solveRound(bool direction, int res_id, double bounding){
    collector.resetTimes();
    collector.startGlobalTime();

    bool preprocess = true;

    if(Parameters::getVerbosity() >= 3) {
        if(round == 0) std::cout<<"--------"<<std::endl;
        std::cout<<"Preprocessing, round: "<< round << std::endl;
    }
    if(preprocessingCritical and res_id == RES_CRITICAL)
        bounding = 0.5;

    dijkstra->initAlgorithm(direction, res_id, bounding);
    dijkstra->solve();

    if(res_id == RES_COST)
        lower_bound = dijkstra->getDistance(direction? problem->getDestination() : problem->getOrigin());

    if(problem->getStatus() != PROBLEM_INFEASIBLE){
        if(dijkstra->foundOptimal()){
            dijkstra->managePaths();
            auto bestPath = dijkstra->getBestSolution();
            addSolution(*bestPath);
            preprocess = false;
        }
        else {
            incumbent = dijkstra->getIncumbent();
            preprocessingCritical and res_id == RES_CRITICAL ? pruneCritical() : pruning();
            if (Parameters::getVerbosity() >= 3) {
                std::cout << "Incumbent: " << incumbent << std::endl;
                std::cout << "--------" << std::endl;
            }
            dijkstra->resetAlgorithm(0);
            round++;
        }
    }
    else preprocess = false;

    collector.stopGlobalTime();
    iterations++;
    collector.collect("iterations", iterations);

    collectData();
    return preprocess;
}

void Preprocessing::resetAlgorithm(int reset_level) {
    problem->resetBoundLabels();
    problem->resetActiveNodes();
    setStatus(ALGO_READY);
}

//Computes a budget split for the search procedure
double Preprocessing::getSplit() {
    double fw_cost = 0, bw_cost = 0;
    BoundLabels* bound_labels = problem->getBoundLabels();
    if(bound_labels->getLabel(RES_COST, true, problem->getOrigin())->getObjective() == MAX_COST)
        return 0.5;

    for (int node = 0; node < problem->getNumNodes(); node++) {

        if(not problem->isActiveNode(node))
            continue;

        fw_cost += bound_labels->getLabel(RES_COST, false, node)->getObjective();
        bw_cost += bound_labels->getLabel(RES_COST, true, node)->getObjective();
    }

    if(fw_cost > bw_cost)
        return std::min(1.0, (0.5 * fw_cost / bw_cost));
    else
        return 1 - std::min(1.0, (0.5 * bw_cost / fw_cost));

}

/** Pruning management **/
//Prunes unreachable nodes
void Preprocessing::pruning() {
    problem->pruneUnreachableNodes(dijkstra->getVisited());
    if(Parameters::getVerbosity() >= 3)
        std::cout<<"Active nodes count after pruning " << problem->countActiveNodes()  << std::endl;
}

//Prunes unreachable nodes when using the critical resource
void Preprocessing::pruneCritical(){
    criticalActive |= dijkstra->getVisited();

    if(round > 0)
        problem->pruneUnreachableNodes(criticalActive);
    if(Parameters::getVerbosity() >= 3)
        std::cout<<"Active nodes count after pruning: " << problem->countActiveNodes()  << std::endl;
}

/** Data collection management **/
void Preprocessing::initDataCollection() {
    collector.setCollectionName("Preprocessing");
    collector.collect("executionID", executionID);

    if(not Parameters::isCollecting())
        return;

    name = "Preprocessing";
    algo_type = "preprocess";

    collector.collect("algo_name", name);
    collector.collect("algo_type", algo_type);
    collector.collect("bidirectional", true);
    collector.init("active_nodes", problem->countActiveNodes());
    collector.setHeader();
}


void Preprocessing::collectData() {
    collector.collect("lb", lower_bound); //update lb too
    collector.collect("ub", incumbent);
    collector.collect("algo_status", algo_status);
    collector.collect("active_nodes", problem->countActiveNodes());

    collector.saveRecord();
}