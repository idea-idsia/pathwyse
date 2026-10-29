#include "PW_default.h"
#include <thread>
#include <fstream>
#include <sstream>
#include "utils/logger.h"

/** Algorithm management **/
//Constructors and destructors

PWDefault::PWDefault(std::string name, Problem* problem): Algorithm(name, problem){
    label_manager = new LMDefault(problem);
    readConfiguration();
    initDataCollection();
    setStatus(ALGO_READY);
}

PWDefault::~PWDefault() {
    writeData();
    delete label_manager;
}

//Init and reset
void PWDefault::initAlgorithm() {
    //Data collection
    unreachable_max_count = 0;

    if(use_visited) {
        //Determines which nodes will be checked for unreachability. Each node has its own bitset. Default value: 0.
        unreachable_active.resize(problem->getNumNodes(), Bitset(problem->getNumNodes()));
        if (ng != NG_OFF or dssr != DSSR_OFF) {
            //If a relaxation is used, the relaxation technique decides which unreachable nodes will be checked
            if (ng != NG_OFF)
                //If NG-route relaxation is active, build ng-neighbourhood
                    buildNG();
        }
        else {
            //if no relaxation is used, all unreachable nodes will be checked
            for(auto & u: unreachable_active)
                u.set();
            unreachable_max_count = problem->getNumNodes();
        }
    }
}

void PWDefault::readConfiguration(){
    requested_solutions = Parameters::getRequestedSolutions();
    timelimit = Parameters::getDefaultTimelimit();
    parallel = Parameters::isDefaultParallel();
    search_direction = Parameters::getDefaultSearch();
    bidirectional = search_direction == SEARCH_BIDIRECTIONAL;
    dssr = Parameters::getDefaultDSSR();
    ng = Parameters::getDefaultNG();
    ng_size = Parameters::getDefaultNGSize();
    ng_set_path = Parameters::getDefaultNGSetPath();
    earlyjoin = Parameters::isDefaultJoinEarly();
    earlyjoin_step = Parameters::getDefaultJoinStep();
    use_visited = label_manager->getUseVisited();
    two_cycle_elimination = Parameters::useTwoCycleElimination();
    algo_type = ALGO_EXACT;

    if(not problem->isGraphCyclic()) {
        dssr = DSSR_OFF;
        ng = NG_OFF;
        use_visited = false;
        label_manager->setUseVisited(use_visited);
        label_manager->setCompareUnreachables(false);
    }
    else {
        if (not use_visited) algo_type = ALGO_RELAXATION;
        else if (not label_manager->getCompareUnreachables()) algo_type = ALGO_HEURISTIC;
    }

    //Pairs are handled in forward search only
    if(problem->hasPairs() and search_direction != SEARCH_FORWARD) {
        Logger::warn("Pickup-delivery pairs require forward search. Switching to forward search.");
        search_direction = SEARCH_FORWARD;
        bidirectional = false;
        label_manager->setForwardSearch();
    }

    if(name == "PWDefaultRelaxDom") {
        dssr = DSSR_OFF;
        ng = NG_OFF;
        algo_type = ALGO_HEURISTIC;
        label_manager->setCompareUnreachables(false);
    }
    else if (name == "PWDefaultRelaxQueue") {
        dssr = DSSR_OFF;
        ng = NG_OFF;
        algo_type = ALGO_HEURISTIC;
        label_manager->setQueueLimit(Parameters::getDefaultRelaxationQueueLimit());
    }
}

void PWDefault::resetIteration(){
    //Reset data at each iteration
    iterations++;
    collector.resetTimes();
    previous_unreachable_max_count = unreachable_max_count;
    it_ext_fw = it_ext_bw = 0;
    label_manager->setExecutionID(executionID);
    label_manager->setIteration(iterations);
    label_manager->initLM();
}

void PWDefault::resetAlgorithm(int reset_level) {
    //Initializes bounds
    Algorithm::initAlgorithm();
    setStatus(ALGO_READY);

    //Resets configuration
    unreachable_active.clear();
    unreachable_ng.clear();
    readConfiguration();

    //Resets solutions
    best_solution_id = -1;
    solutions.clear();

    //Resets LM
    label_manager->resetLM();

    //Resets data collection and extra parameters
    collector.resetTimesCumulative();
    timeout = false;
}

bool PWDefault::preprocessing(){
    bool termination = true;

    preprocess = new Preprocessing("preprocessing", problem);
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
        termination = false;
        label_manager->setSplit(preprocess->getSplit());
        label_manager->setIncumbent(incumbent);
        label_manager->initLM();
        label_manager->setExecutionID(executionID);
    }

    if (timeout) setStatus(ALGO_TIMELIMIT);
    else if(termination) setStatus(ALGO_DONE);
    return termination;
}

//Solve problem
void PWDefault::solve(){

    setStatus(ALGO_OPTIMIZING);

    bool termination = false;


    if(not problem->isGraphCyclic() and Parameters::getPreprocessingIntensity() != PREPROCESSING_OFF){
        collector.startGlobalTime();
        termination = preprocessing();
        collector.stopGlobalTime();
        if(termination) {
            collectSolution(best_solution_id);
            collectData();
        }
    }

    initAlgorithm();

    while(not termination) {
        collector.startGlobalTime();
        resetIteration();

        //Labeling
        collector.startTime("t_labeling");
        if (parallel and bidirectional) {
            std::thread fw(&PWDefault::labeling, this, true, false);    //Forward labeling
            std::thread bw(&PWDefault::labeling, this, false, true);    //Backward labeling
            fw.join();
            bw.join();
        }
        else if (search_direction == SEARCH_FORWARD) labeling(true, false);
        else if (search_direction == SEARCH_BACKWARD) labeling(false, true);
        else labeling(true, true);


        collector.stopTime("t_labeling");

        //If bidirectional search is over, join labels
        if (not timeout and bidirectional) {
            label_manager->update_split();                              //Dynamic critical resource budget update

            //Join procedure
            collector.startTime("t_join");
            label_manager->join();
            collector.stopTime("t_join");
        }

        //Manage paths
        managePaths();
        label_manager->collectData();

        //Check Termination
        termination = checkTermination();

        if(termination and requested_solutions > 1)
            manageAdditionalPaths();

        collector.stopGlobalTime();

        if (timeout) setStatus(ALGO_TIMELIMIT);
        else if(termination) setStatus(ALGO_DONE);
        collectData();
    }

}


void PWDefault::labeling(bool forward, bool backward) {
    //If candidates are available
    while(label_manager->candidatesAvailable(forward, backward)) {
        if(isTimeLimitReached()) return;


        //Get candidate label from label manager
        LabelAdv *candidate = label_manager->getCandidate(forward, backward);

        //Extend label towards reachable nodes
        collector.startTime(candidate->getDirection() ? "t_ext_fw" : "t_ext_bw");
        extend(candidate);
        collector.stopTime(candidate->getDirection() ? "t_ext_fw" : "t_ext_bw");
        candidate->getDirection() ? it_ext_fw++ : it_ext_bw++;

        //Early Join
        if(forward and backward and bidirectional and earlyjoin and
                earlyjoin_step <= label_manager->totalLabels()) {
            if(Parameters::getVerbosity() >= 2)
                std::cout << "Early join" << std::endl;

            if(earlyjoin_step < MAX_JOIN_STEP)
                earlyjoin_step *= earlyjoin_step;
            else
                earlyjoin = false;

            collector.startTime("t_join");
            label_manager->join();
            collector.stopTime("t_join");
        }
    }
}


void PWDefault::extend(LabelAdv* candidate) {
    LabelAdv new_label = LabelAdv();
    int node = candidate->getNode();
    bool direction = candidate->getDirection();
    bool active;

    std::vector<int> neighbors = (candidate->getExtensionTarget() == ALL)
        ? problem->getNeighbors(node, candidate->getDirection())
        : std::vector<int>{candidate->getExtensionTarget()};

    for(auto & neigh: neighbors) {

        //Only extends towards active nodes
        if(not problem->isActiveNode(neigh))
            continue;

        //No coming back to origin (forward) or destination (backward)
        if((direction and neigh == problem->getOrigin()) or (not direction and neigh == problem->getDestination()))
            continue;

        //Eliminates k = 2 cycles
        if(two_cycle_elimination and candidate->getPredecessorNode() == neigh)
            continue;

        //Pickup-delivery pairs
        if(problem->hasPairs() and not label_manager->isPairingFeasible(candidate, neigh))
            continue;

        //Unreachable bits cover monotone resources only: non-monotone resources are checked directly.
        //The start label has no bits yet, so its extensions are checked in full.
        active = not unreachable_active.empty() and unreachable_active[node].get(neigh);
        bool is_start = candidate->getPredecessorNode() < 0;
        bool feasible = (active and not is_start)
            ? label_manager->isNodeReachable(candidate, neigh) and
              label_manager->isExtensionFeasible(candidate, neigh, SCOPE_NONMONOTONE)
            : label_manager->isExtensionFeasible(candidate, neigh);

        if(feasible){

            //critical res extension check
            if(bidirectional and not label_manager->isCriticalExtensionFeasible(candidate, neigh))
                continue;

            //For each reachable node, extend label in that direction
            label_manager->extendLabel(candidate, &new_label, neigh);

            //Update unreachables
            label_manager->updateUnreachables(&new_label);
            if(not unreachable_active.empty())
                new_label.updateUnreachables(unreachable_active[neigh]);

            if(Parameters::getCollectionLevel()>=1)
                collector.startTime(direction ? T_INS_FW : T_INS_BW);
            label_manager->insert(&new_label);
            if(Parameters::getCollectionLevel()>=1)
                collector.stopTime(direction ? T_INS_FW : T_INS_BW);
        }
    }
}

bool PWDefault::checkTermination() {

    bool termination = true;
    auto bestPath = getBestSolution();

    //Condition 1: Timeout reached
    if(timeout) {
        if(bestPath and bestPath->isElementary())
            updateIncumbent(bestPath->getObjective());

        return termination;
    }

    //Condition 2: No solution found
    if(not bestPath) {
        if(algo_type == ALGO_EXACT)
            problem->setStatus(PROBLEM_INFEASIBLE);
        return termination;
    }

    if(algo_type == ALGO_EXACT)
        updateLowerBound(bestPath->getObjective());

    //Condition 3: Elementary solution found
    if(bestPath->isElementary()){
        updateIncumbent(bestPath->getObjective());

        if(ng != NG_OFF)
            ng_compliant = true;
        return termination;
    }

    //Condition 4: Non-elementary super optimal solution found
    if (algo_type == ALGO_RELAXATION and not use_visited)
        return termination;

    //Condition 5: NG solution found (if DSSR is OFF)
    if(ng == NG_RESTRICTED) {
        termination = NgRestricted();
        ng_compliant = termination;
    }

    //If DSSR is enabled, termination will be false until an elementary solution is found
    if(dssr != DSSR_OFF and termination) {
        //NG algorithms are not used anymore
        ng = NG_OFF;
        termination = dssr == DSSR_STANDARD ? DssrStandard() : DssrRestricted();
    }

    //Reset for next iteration
    if(not termination) {
        label_manager->resetLM();
        clearSolutions();
    }

    return termination;
}

void PWDefault::managePaths(){
    isTimeLimitReached();

    auto solution_data = label_manager->getSolutionLabels();

    int objective = std::get<0>(solution_data);
    auto *fw = std::get<1>(solution_data);
    auto *bw = std::get<2>(solution_data);
    buildPath(objective, fw, bw);

    auto bestPath = getBestSolution();

    if(not bestPath)
        return;

    if(bestPath->isElementary()){
        problem->setStatus(PROBLEM_FEASIBLE);
        if((algo_type == ALGO_EXACT or algo_type == ALGO_RELAXATION) and not timeout)
            bestPath->setStatus(PATH_OPTIMAL);
        else
            bestPath->setStatus(PATH_FEASIBLE);
    }
    else if (ng != NG_OFF and isNGCompliant(*bestPath)) bestPath->setStatus(PATH_NG);
    else bestPath->setStatus(PATH_SUPEROPTIMAL);

    std::list<LabelAdv> tourFW = label_manager->buildTour(bestPath->getTour(), true);
    bestPath->setConsumption(tourFW.back().getSnapshot());
    collectSolution(best_solution_id);
}

void PWDefault::manageAdditionalPaths(){
    int additional_solutions_found = 0;

    auto buildAdditionalPaths = [&](int cost, LabelAdv* fw, LabelAdv* bw) {
        buildPath(cost, fw, bw);
        auto& path = solutions.back();

        bool valid = path.isElementary();

        if(valid and getBestSolution()->getStatus() == PATH_OPTIMAL and getBestSolution()->getObjective() == path.getObjective())
            path.setStatus(PATH_OPTIMAL);
        else if(valid) path.setStatus(PATH_FEASIBLE);

        if(not valid and ng != NG_OFF) {
            valid = isNGCompliant(path);
            if(valid) path.setStatus(PATH_NG);
        }

        if(not valid) { solutions.resize(solutions.size() - 1); return; }

        std::list<LabelAdv> tourFW = label_manager->buildTour(path.getTour(), true);
        path.setConsumption(tourFW.back().getSnapshot());
        collectSolution(solutions.size() - 1);
        additional_solutions_found++;
    };

    if(bidirectional) {
        if(not label_manager->joinFound()) return;
        bool first_sol = true;
        for(auto& [cost, fw, bw]: label_manager->getAllJoin()) {
            if(first_sol) { first_sol = false; continue; }
            if(additional_solutions_found >= requested_solutions - 1) break;
            buildAdditionalPaths(cost, fw, bw);
        }
    } else {
        bool is_forward = (search_direction != SEARCH_BACKWARD);
        int target = is_forward ? problem->getDestination() : problem->getOrigin();
        LabelAdv* best_od = label_manager->getODLabel();
        label_manager->sortClosedLabels(is_forward, target);
        for(auto& [cost, idx]: label_manager->getClosedLabels(is_forward, target)) {
            if(additional_solutions_found >= requested_solutions - 1) break;
            LabelAdv* label = label_manager->getLabel(is_forward, idx);
            if(label == best_od) { best_od = nullptr; continue; }   // skip the label already used by managePaths
            LabelAdv* fw = is_forward ? label : nullptr;
            LabelAdv* bw = is_forward ? nullptr : label;
            buildAdditionalPaths(cost, fw, bw);
        }
    }
}

/** Relaxation management **/
bool PWDefault::DssrStandard(){
    bool isElementary = true;

    Bitset repeated_visits(problem->getNumNodes());
    auto tour = getBestSolution()->getTour();
    tour.sort();
    int current_node = -1;

    for(auto t: tour) {
        if(t == current_node) {
            isElementary = false;
            repeated_visits.set(current_node);
        }
        else current_node = t;
    }

    //update unreachables
    int u_count;
    if(not isElementary) {
        for(auto & u: unreachable_active) {
            u |= repeated_visits;
            u_count = u.count();
            if(u_count > unreachable_max_count)
                unreachable_max_count = u_count;
        }
    }

    return isElementary;
}

bool PWDefault::DssrRestricted(){
    bool isElementary = true;

    auto tour = getBestSolution()->getTour();
    int scanned_node;
    std::list<int> scanned_nodes;
    Bitset unique_visits(problem->getNumNodes()), cycle_visits(problem->getNumNodes());

    int current_node;
    for(auto it = tour.begin(); it != tour.end(); it++) {
        current_node = *it;
        if(unique_visits.get(current_node)){
            for(auto it_scan = scanned_nodes.rbegin(); it_scan != scanned_nodes.rend(); it_scan++) {
                scanned_node = *it_scan;
                cycle_visits.set(scanned_node);
                if(current_node == scanned_node) {
                    isElementary = false;

                    for(int i = 0; i < cycle_visits.size(); i++)
                        if(cycle_visits.get(i))
                            unreachable_active[i].set(current_node);

                    break;
                }
            }
            cycle_visits.reset();
        }
        else unique_visits.set(current_node);
        scanned_nodes.push_back(current_node);
    }

    int u_count;
    for(auto t: tour){
        u_count = unreachable_active[t].count();
        if(u_count > unreachable_max_count)
            unreachable_max_count = u_count;
    }

    return isElementary;
}

void PWDefault::buildNG(){
    ng_compliant = true;
    int n = problem->getNumNodes();
    unreachable_ng.resize(n, Bitset(n));

    if(not ng_set_path.empty()) {
        std::ifstream f(ng_set_path);
        std::string line, token;
        for(int i = 0; i < n; i++) {
            std::getline(f, line);
            std::istringstream ss(line);
            for(int j = 0; j < n; j++) {
                std::getline(ss, token, ',');
                if(std::stoi(token)) unreachable_ng[i].set(j);
            }
        }
    } 
    else {
        if(ng_size <= 1) return;

        auto objective = problem->getObj();
        std::set<std::pair<int, int>> ng_neighbors;

        int neighborhood_max_size = ng_size - 1; //To account for node i in the set
        for(int i = 0; i < n; i++) {
            for(int j = 0; j < n; j++)
                if(problem->areNeighbors(i, j, true)) {
                    int extension_cost = objective->extend(0,i,j,true);
                    if(ng_neighbors.size() < neighborhood_max_size)
                        ng_neighbors.insert(std::make_pair(extension_cost, j));
                    else if(extension_cost < ng_neighbors.rbegin()->first) {
                            ng_neighbors.erase(std::prev(ng_neighbors.end()));
                            ng_neighbors.insert(std::make_pair(extension_cost, j));
                    }
                }

            for(auto & nb: ng_neighbors)
                unreachable_ng[i].set(nb.second);
            unreachable_ng[i].set(i);

            ng_neighbors.clear();
        }
    }

    if(ng == NG_STANDARD) {
        unreachable_active = unreachable_ng;
        unreachable_max_count = unreachable_active[0].count();
    }
}

bool PWDefault::NgRestricted() {
    bool isNG = true;
    Bitset visited_memory(problem->getNumNodes());
    auto tour = getBestSolution()->getTour();
    std::list<int> scanned_nodes;
    int scanned_node;

    for(auto t: tour) {
        //If visited has memory of seeing a node that is part of the ng-route: cycle on t is forbidden
        if(visited_memory.get(t)){
            isNG = false;
            //Bit to 1 in position t for each node of the closest cycle
            for(auto it_scan = scanned_nodes.rbegin(); it_scan != scanned_nodes.rend(); it_scan++) {
                scanned_node = *it_scan;
                unreachable_active[scanned_node].set(t);
                if(scanned_node == t) break;
            }
        }
        scanned_nodes.push_back(t);
        visited_memory &= unreachable_ng[t];
        visited_memory.set(t);
    }

    int u_count;
    for(auto t: tour){
        u_count = unreachable_active[t].count();
        if(u_count > unreachable_max_count)
            unreachable_max_count = u_count;
    }

    return isNG;
}

bool PWDefault::isNGCompliant(Path& path) {
    // For each arc (i, j): the extension is forbidden if j is in the current memory.
    // After visiting i: memory = (memory ∩ NG(i)) ∪ {i}.
    Bitset memory(problem->getNumNodes());
    for(auto node: path.getTour()) {
        if(memory.get(node)) return false;
        memory &= unreachable_ng[node];
        memory.set(node);
    }
    return true;
}

std::string PWDefault::getRelaxationName() {
    std::string ng_name, dssr_name, relaxation_name;

    switch(ng){
        case NG_STANDARD: ng_name = "NG standard"; break;
        case NG_RESTRICTED: ng_name = "NG restricted"; break;
    }

    switch(dssr){
        case DSSR_STANDARD: dssr_name = "DSSR standard"; break;
        case DSSR_RESTRICTED: dssr_name = "DSSR restricted"; break;
    }

    relaxation_name = ng_name;
    if(not relaxation_name.empty() and not dssr_name.empty())
        relaxation_name += " + ";
    relaxation_name += dssr_name;

    return relaxation_name.empty() ? "no relaxation" : relaxation_name;
}


/** Debug **/
void PWDefault::preGenLabels(std::list<int> tour, bool direction){
    if(not direction)
        tour.reverse();
    tour.pop_front();
    int next_node;

    //Starting label from origin or destination
    LabelAdv* current_label = label_manager->getLabel(direction, 0);
    LabelAdv new_label = LabelAdv();

    while(not tour.empty()) {
        next_node = tour.front();
        tour.pop_front();
        label_manager->extendLabel(current_label, &new_label, next_node);
        label_manager->updateUnreachables(&new_label);
        current_label = label_manager->insert(&new_label);
    }
}

/** Data Collection management **/
void PWDefault::initDataCollection() {
    timeout = false;

    collector.setCollectionName(name);

    if(not Parameters::isCollecting())
        return;

    label_manager->setName(name + "_LM");

    collector.collect("algo_name", name);
    collector.collect("algo_type", algo_type);
    if(bidirectional)
        collector.collect("bidirectional", true);

    //Initialize data collection of new values
    collector.initTime("t_labeling");
    collector.initTime("t_ext_fw");
    collector.initTime("t_ext_bw");
    if(Parameters::getCollectionLevel() >= 1){
        collector.initTime("t_ins_fw");
        collector.initTime("t_ins_bw");
    }
    collector.initTime("t_join");
    collector.init("use_visited", -1);
    collector.init("it_ext_fw", 0);
    collector.init("it_ext_bw", 0);
    collector.init("relaxation", getRelaxationName());
    collector.init("neighbourhood_max_size", unreachable_max_count);

    collector.init("NGroute_compliant", -1);

    collector.setHeader();
}

void PWDefault::collectData(){
    collector.collect("executionID", executionID);

    //Collect Data at the end of iteration
    if(not Parameters::isCollecting())
        return;

    collector.collect("use_visited", use_visited);
    collector.collect("iterations", iterations);
    collector.collect("it_ext_fw", it_ext_fw);
    collector.collect("it_ext_bw", it_ext_bw);
    collector.collect("lb", lower_bound);
    collector.collect("ub", incumbent);
    collector.collect("algo_status", algo_status);
    if(ng != NG_OFF)
        collector.collect("NGroute_compliant", ng_compliant);
    collector.collect("timeout", timeout);
    collector.collect("neighbourhood_max_size", previous_unreachable_max_count);

    collector.saveRecord();
}

void PWDefault::writeData(){
    collector.writeData();
    collector_sol.writeData();
    label_manager->writeData();
}