#include "PW_acyclic.h"

/** Algorithm management **/
//Constructors and destructors
PWAcyclic::PWAcyclic(std::string name, Problem* problem): Algorithm(name, problem){
    preprocess = new Preprocessing(name, problem);
    label_manager = new LMacyclic(problem);
    initDataCollection();
    setStatus(ALGO_READY);
}

PWAcyclic::~PWAcyclic(){
    delete label_manager;
    delete preprocess;
}

//Initialization
void PWAcyclic::initAlgorithm() {
    it_dominated = 0;
    it_ext_fw = it_ext_bw = 0;
    ins_attempts_fw = ins_attempts_bw = 0;
    timelimit = Parameters::getDefaultTimelimit();
}

//Perform preprocessing
bool PWAcyclic::preprocessing(){
    bool search_required = false;

    preprocess->setExecutionID(executionID);
    preprocess->solve();
    lower_bound = preprocess->getLowerBound();
    incumbent = preprocess->getIncumbent();

    if(Parameters::getVerbosity() >= 1)
        std::cout << "Preprocessing time (s): " << preprocess->getGlobalTime() << std::endl;

    auto bestPath = preprocess->getBestSolution();
    if(bestPath and bestPath->getStatus() == PATH_OPTIMAL) {
        addSolution(*bestPath);
        if(Parameters::getVerbosity() >= 2)
            std::cout<<"Optimal solution found during pre-processing...terminating"<<std::endl;
    }
    else if(problem->getStatus() != PROBLEM_INFEASIBLE) {
        if(Parameters::getVerbosity() >= 2)
            std::cout<<"Pre-processing complete...searching for an optimal solution"<<std::endl;
        search_required = true;

        label_manager->setSplit(preprocess->getSplit());
        label_manager->setIncumbent(incumbent);
        label_manager->initLM();
        label_manager->setExecutionID(executionID);
    }
    return search_required;
}

//Solve
void PWAcyclic::solve() {
    setStatus(ALGO_OPTIMIZING);

    if(problem->isGraphCyclic()) {
        std::cout<<"Terminating: PWAcyclic is not suitable for cyclic problems. Please, choose another algorithm."<<std::endl;
        return;
    }
    if(problem->getNumRes() > 1) {
        std::cout<<"Terminating: PWAcyclic is not suitable for instances with more than one resource. Please, choose another algorithm."<<std::endl;
        return;
    }
    if(problem->hasNegativeCost()) {
        std::cout<<"Terminating: PWAcyclic is not suitable for instances with negative costs. Please, choose another algorithm."<<std::endl;
        return;
    }
    collector.startGlobalTime();

    initAlgorithm();

    bool search = preprocessing();
    if(search) {
        Label* candidate;
        while(label_manager->candidatesAvailable()){
            if (isTimeLimitReached()) break;
            candidate = label_manager->getCandidate();
            candidate->getDirection() ? it_ext_fw++ : it_ext_bw++;

            bool dominated = label_manager->isDominated(candidate);
            if(not dominated) {
                label_manager->earlyUpdate(candidate);
                label_manager->closeLabel(candidate);

                if(label_manager->isExtensionFeasible(candidate))
                    extend(candidate);

                label_manager->join(candidate);
            }
            else {
                it_dominated++;
                label_manager->eraseCandidate(candidate);
            }
        }
        managePaths();
    }

    collector.stopGlobalTime();

    if(algo_status == ALGO_OPTIMIZING)
        setStatus(ALGO_DONE);

    collectData();
}

//Extend a candidate towards its neighbours
void PWAcyclic::extend(Label* candidate) {
    Label new_label = Label();
    int node = candidate->getNode();
    bool direction =  candidate->getDirection();
    auto & neighbours = problem->getNeighbors(node, direction);
    for(auto & next: neighbours){
        if(problem->isActiveNode(next)) {
            direction? ins_attempts_fw++ : ins_attempts_bw++;
            label_manager->extendLabel(candidate, new_label, next);
            label_manager->insert(new_label);
        }
    }
}

//Builds a path from two labelss
void PWAcyclic::managePaths(){
    if (timeout) return;
    auto [objective, l1, l2] = *label_manager->getJoin();

    auto fw = l1.getDirection() ? &l1 : &l2;
    auto bw = l1.getDirection() ? &l2 : &l1;

    if(fw and bw and fw->getNode() == bw->getNode())
        fw = fw->getPredecessor(); //Removes duplicate node when performing node join

    buildPath(objective, fw, bw);

    auto bestPath = getBestSolution();

    if(not bestPath)
        return;

    bestPath->setStatus(PATH_OPTIMAL);

    std::list<Label> tourFW = label_manager->buildTour(bestPath->getTour(), true);
    bestPath->setConsumption(tourFW.back().getSnapshot());
}

//Reset
void PWAcyclic::resetAlgorithm(int reset_level){
    preprocess->resetAlgorithm(reset_level);
    Algorithm::initAlgorithm();
    best_solution_id = -1;
    solutions.clear();
    label_manager->resetLM();
    setStatus(ALGO_READY);
}

/** Data collection **/
void PWAcyclic::initDataCollection() {
    collector.setCollectionName("PWAcyclic");

    if(not Parameters::isCollecting())
        return;

    name = "PWAcyclic";
    algo_type = "exact";
    label_manager->setName(name + "_LM");

    collector.collect("algo_name", name);
    collector.collect("algo_type", algo_type);
    collector.collect("bidirectional", true);

    collector.init("it_ext_fw", 0);
    collector.init("it_ext_bw", 0);
    collector.init("it_dominated", 0);

    collector.init("ins_attempts_fw", 0);
    collector.init("ins_attempts_bw", 0);

    collector.setHeader();
}


void PWAcyclic::collectData(){
    collector.collect("executionID", executionID);

    if(not Parameters::isCollecting())
        return;

    collector.collect("iterations", ++iterations);
    collector.collect("lb", lower_bound);
    collector.collect("ub", incumbent);
    collector.collect("it_ext_fw", it_ext_fw);
    collector.collect("it_ext_bw", it_ext_bw);
    collector.collect("it_dominated", it_dominated);
    collector.collect("ins_attempts_fw", ins_attempts_fw);
    collector.collect("ins_attempts_bw", ins_attempts_bw);
    collector.collect("algo_status", algo_status);
    collector.saveRecord();
    collector.writeData();

    if(best_solution_id >= 0)
        collectSolution(best_solution_id);
    collector_sol.writeData();

    if(it_ext_fw + it_ext_bw > 0) {
        label_manager->collectData();
        label_manager->writeData();
    }
}
