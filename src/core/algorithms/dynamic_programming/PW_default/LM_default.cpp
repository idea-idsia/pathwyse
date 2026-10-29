#include "LM_default.h"
#include <sstream>
#include <algorithm>

/** LM management **/
//Constructors and destructors
LMDefault::LMDefault(Problem* problem) {
    executionID = 0;
    name = "label_manager";
    lm_type = "pqueue";
    this->problem = problem;
    queue_limit = UNKNOWN;

    readConfiguration();

    collector = DataCollector(name);
    collector_label_fw = DataCollector("Labels_fw");
    collector_label_bw = DataCollector("Labels_bw");

    initDataCollection();

    best_fw = nullptr;
    best_bw = nullptr;
    iterations = 0;
    ins_attempts_fw = ins_attempts_bw = 0;
}


//Init
void LMDefault::initLM(){
    //Get data from problem
    auto objective = problem->getObj();
    std::vector<Resource*>& resources = problem->getResources();

    int origin = problem->getOrigin();
    int destination = problem->getDestination();
    int n_nodes = problem->getNumNodes();
    int n_res = problem->getNumRes();

    computeMonotoneResources();

    //Reserve space for labels
    forward_labels.reserve(reserve_size);
    backward_labels.reserve(reserve_size);
    ndominated_fw = ndominated_bw = nclosed_fw = nclosed_bw = 0;

    //Data structures initialization
    forward_candidates.resize(n_nodes, std::list<std::pair<int, int>>());
    backward_candidates.resize(n_nodes, std::list<std::pair<int, int>>());
    forward_closed.resize(n_nodes, std::vector<std::pair<int, int>>());
    backward_closed.resize(n_nodes, std::vector<std::pair<int, int>>());
    forward_best.resize(n_nodes, nullptr);
    backward_best.resize(n_nodes, nullptr);

    //Push back first forward and backward labels
    bool direction = true;
    LabelAdv* predecessor = nullptr;
    bool forward_ok = search_direction == SEARCH_BIDIRECTIONAL or search_direction == SEARCH_FORWARD;
    bool backward_ok = search_direction == SEARCH_BIDIRECTIONAL or search_direction == SEARCH_BACKWARD;

    //Initialize snapshots
    if (forward_ok)
    {
        forward_labels.emplace_back();
        forward_labels[0].initLabel(origin, predecessor, direction, n_res);
        if(use_visited)
            forward_labels[0].initVisited(origin, n_nodes);
        if(problem->hasPairs())
            forward_labels[0].ensureOpen(n_nodes);
        forward_labels[0].setObjective(objective->getInitValue() + objective->getNodeCost(origin));
        ++ins_attempts_fw;

        for(int id = 0; id < problem->getNumRes(); id++)
            forward_labels[0].setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(origin));

        if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored()) {
            collectLabel(&collector_label_fw, &forward_labels[0]);
            collector_label_fw.addLastRecordToSubset();
        }

        forward_candidates[origin].emplace_back(std::make_pair(forward_labels[0].getObjective(), 0));
        forward_best[origin] = & forward_labels[0];
    }
    if (backward_ok){
        backward_labels.emplace_back();
        backward_labels[0].initLabel(destination, predecessor, !direction, n_res);
        if(use_visited)
            backward_labels[0].initVisited(destination, n_nodes);
        backward_labels[0].setObjective(objective->getInitValue() + objective->getNodeCost(destination));
        ++ins_attempts_bw;

        for(int id = 0; id < problem->getNumRes(); id++)
            backward_labels[0].setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(destination));

        if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored()){
            collectLabel(&collector_label_bw, &backward_labels[0]);
            collector_label_bw.addLastRecordToSubset();
        }
        backward_candidates[destination].emplace_back(std::make_pair(backward_labels[0].getObjective(), 0));
        backward_best[destination] = & backward_labels[0];

    }

    incumbent = UNKNOWN;
    turn_forward = origin;
    turn_backward = destination;
    od_label.second = -1;
    joinComparisons = 0;
}

void LMDefault::readConfiguration() {
    search_direction = Parameters::getDefaultSearch();
    bidirectional = search_direction == SEARCH_BIDIRECTIONAL;
    autoconfiguration = Parameters::isDefaultAutoConfigured();
    split_ratio = Parameters::getDefaultSplit();
    reserve_size = Parameters::getDefaultReserve();
    compare_unreachables = Parameters::isDefaultUsingUnreachables();
    use_visited = compare_unreachables or Parameters::isDefaultUsingVisited();
    two_cycle_elimination = Parameters::useTwoCycleElimination();
    candidate_type = Parameters::getDefaultCandidateType();
    join_algo = Parameters::getDefaultJoinType();
    join_type = (join_algo == JOIN_ORDERED_NODE or join_algo == JOIN_KORDERED_NODE or join_algo == JOIN_PARETO_NODE) ? JOIN_NODE : JOIN_ARC;

    requested_solutions = Parameters::getRequestedSolutions();
    if(autoconfiguration) {
        //Simple auto-tune of some parameters
        bool active = false;
        candidate_type = CANDIDATE_RR;

        if(problem->isGraphCyclic() or Parameters::getDefaultDSSR() != DSSR_OFF or Parameters::getDefaultNG() != NG_OFF){
            active = true;
            candidate_type = CANDIDATE_NODE;
        }

        compare_unreachables = use_visited = active;
        if (bidirectional and requested_solutions > 1 and (join_algo != JOIN_KORDERED or join_algo != JOIN_KORDERED_NODE)){
            join_algo = join_type == JOIN_NODE ? JOIN_KORDERED_NODE : JOIN_KORDERED;
        }
    }
}

void LMDefault::resetLM(){
    forward_labels.clear();
    forward_candidates.clear();
    forward_closed.clear();
    forward_closed_backup.clear();
    forward_best.clear();

    backward_labels.clear();
    backward_candidates.clear();
    backward_closed.clear();
    backward_closed_backup.clear();
    backward_best.clear();

    joinable_labels.clear();
    best_fw = nullptr;
    best_bw = nullptr;
    incumbent = UNKNOWN;
    collector_label_fw.clearSubsetRecords();
    collector_label_bw.clearSubsetRecords();
    ins_attempts_fw = ins_attempts_bw = 0;
}

void LMDefault::update_split(){
    if(Parameters::isCollecting())
        collector.collect("split", split_ratio);

    int f_size = forward_labels.size();
    int b_size = backward_labels.size();

    if(f_size - b_size + f_size*0.2 < 0)
        split_ratio += 0.05;
    else if(b_size - f_size + b_size*0.2 < 0)
        split_ratio -= 0.05;
}

/** Candidate management **/
//Returns true if an open label is available
bool LMDefault::candidatesAvailable(bool forward, bool backward) {

    if(forward and ndominated_fw + nclosed_fw < forward_labels.size())
        return true;
    if(backward and ndominated_bw + nclosed_bw < backward_labels.size())
        return true;

    return false;

    /*
    //Safe (but slower) method
    for(int i = 0; i < forward_candidates.size(); i++) {
        if(forward and !forward_candidates[i].empty())
            return true;
        if(backward and !backward_candidates[i].empty())
            return true;
    }
    return false;
    */
}

//Returns an open candidate
LabelAdv* LMDefault::getCandidate(bool forward, bool backward) {

    LabelAdv* candidate;

    switch(candidate_type){
        default:
            candidate = getCandidateRR(forward, backward);
            break;
        case CANDIDATE_NODE:
            candidate = getCandidateNode(forward, backward);
            break;
    }

    if(candidate != nullptr)
        candidate->getDirection() ? nclosed_fw++ : nclosed_bw++;

    return candidate;
}

//Returns an open candidate in a round-robin fashion
//(eg: min cost open label comes from node 1. Next candidate comes from node 2, then 3 and so on.)
LabelAdv* LMDefault::getCandidateRR(bool forward, bool backward){
    LabelAdv* candidate = nullptr;
    int index, position;
    bool direction;
    int score = UNKNOWN;
    bool foundLabel = false;

    int turn;
    if(forward) turn = turn_forward;
    else turn = turn_backward;

    while(!foundLabel) {

        if(turn >= forward_candidates.size()) turn = 0;

        if(forward and !forward_candidates[turn].empty()){
            score = forward_candidates[turn].begin()->first;
            index = forward_candidates[turn].begin()->second;
            direction = true;
            foundLabel = true;
        }

        if(backward and !backward_candidates[turn].empty() and backward_candidates[turn].begin()->first < score) {
            score = backward_candidates[turn].begin()->first;
            index = backward_candidates[turn].begin()->second;
            direction = false;
            foundLabel = true;
        }
        position = turn;
        turn++;
    }

    if(direction) {
        candidate = & forward_labels[index];
        forward_closed[position].emplace_back(std::make_pair(score,index));
        forward_candidates[position].pop_front();
    }
    else {
        candidate = & backward_labels[index];
        backward_closed[position].emplace_back(std::make_pair(score,index));
        backward_candidates[position].pop_front();
    }

    if(forward) turn_forward = turn;
    if(backward) turn_backward = turn;

    return candidate;

}

//Returns an open label from a certain node until it has no open labels left
LabelAdv* LMDefault::getCandidateNode(bool forward, bool backward) {

    LabelAdv* candidate = nullptr;
    int index, position;
    bool direction;
    int score = UNKNOWN;
    bool foundLabel = false;

    int turn = forward? turn_forward : turn_backward;

    while(!foundLabel) {
        if(forward and backward) {
            if(forward_candidates[turn].empty() and backward_candidates[turn].empty())
                turn++;
        }
        else if((forward and forward_candidates[turn].empty()) or (backward and backward_candidates[turn].empty()))
            turn++;

        if(turn >= forward_candidates.size()) turn = 0;

        if(forward and !forward_candidates[turn].empty()){
            score = forward_candidates[turn].begin()->first;
            index = forward_candidates[turn].begin()->second;
            direction = true;
            foundLabel = true;
        }
        if(backward and !backward_candidates[turn].empty() and backward_candidates[turn].begin()->first < score) {
            score = backward_candidates[turn].begin()->first;
            index = backward_candidates[turn].begin()->second;
            direction = false;
            foundLabel = true;
        }
    }

    if(direction) {
        candidate = & forward_labels[index];
        position = candidate->getNode();
        forward_closed[position].emplace_back(std::make_pair(score,index));
        forward_candidates[position].pop_front();
    }
    else {
        candidate = & backward_labels[index];
        position = candidate->getNode();
        backward_closed[position].emplace_back(std::make_pair(score,index));
        backward_candidates[position].pop_front();
    }

    if(forward) turn_forward = turn;
    if(backward) turn_backward = turn;

    return candidate;

}

/** Label management **/
//Extension
bool LMDefault::isNodeReachable(LabelAdv *label, int next_node){
    if(use_visited)
        return label->isReachable(next_node);
    else
        return isExtensionFeasible(label, next_node);
}

void LMDefault::computeMonotoneResources() {
    std::vector<Resource*>& resources = problem->getResources();
    res_monotone.assign(resources.size(), 1);
    for(int id = 0; id < resources.size(); id++)
        for(int node = 0; node < problem->getNumNodes(); node++)
            if(resources[id]->getNodeCost(node) < 0) {
                res_monotone[id] = 0;
                break;
            }
}

//Pairing and precedence: a delivery needs its pickup open, a pickup cannot be opened twice,
//the destination needs no open pickup. Forward search only.
bool LMDefault::isPairingFeasible(LabelAdv *label, int next_node) {
    if(next_node == problem->getDestination())
        return not label->hasOpen();
    if(problem->isPickup(next_node))
        return not label->isOpen(next_node);
    int pickup = problem->getPickupOf(next_node);
    return pickup < 0 or label->isOpen(pickup);
}

bool LMDefault::isExtensionFeasible(LabelAdv *label, int next_node, int scope) {
    int current_value;
    bool direction = label->getDirection();
    double bounding = 1;

    std::vector<Resource*>& resources = problem->getResources();

    if(scope != SCOPE_ALL and res_monotone.size() != resources.size())
        computeMonotoneResources();

    int i = direction ? label->getNode() : next_node;
    int j = direction ? next_node : label->getNode();

    for(int id = 0; id < resources.size(); id++) {
        if(scope != SCOPE_ALL and (scope == SCOPE_MONOTONE) != (bool)res_monotone[id])
            continue;
        current_value = label->getSnapshot(id);
        current_value = resources[id]->extend(current_value, i, j, direction);
        if(!resources[id]->isFeasible(current_value, next_node, bounding, direction))
            return false;
    }

    return true;
}

bool LMDefault::isCriticalExtensionFeasible(LabelAdv *label, int next_node) {
    int current_value;
    bool direction = label->getDirection();
    double bounding = direction ? split_ratio : 1 - split_ratio;

    std::vector<Resource*>& resources = problem->getResources();

    int i = direction ? label->getNode() : next_node;
    int j = direction ? next_node : label->getNode();

    current_value = label->getSnapshot(RES_CRITICAL);

    if(join_type == JOIN_ARC and direction)
        current_value = resources[RES_CRITICAL]->extend(current_value, i, j, direction);

    if(not resources[RES_CRITICAL]->isFeasible(current_value, next_node, bounding, direction))
        return false;

    return true;
}

void LMDefault::extendLabel(LabelAdv *current_label, LabelAdv *new_label, int next_node) {
    auto objective = problem->getObj();
    std::vector<Resource*>& resources = problem->getResources();
    *new_label = *current_label;
    new_label->updateLabel(next_node, current_label);
    bool direction = current_label->getDirection();

    int i = direction ? current_label->getNode() : next_node;
    int j = direction ? next_node : current_label->getNode();

    //Update Objective
    int current_value, new_value;
    current_value = current_label->getObjective();
    new_value = objective->extend(current_value, i, j, direction);
    new_label->setObjective(new_value);

    //Update resources
    for(int id = 0; id < problem->getNumRes(); id++) {
        current_value = current_label->getSnapshot(id);
        new_value = resources[id]->extend(current_value, i, j, direction);
        new_label->setSnapshot(id, new_value);
    }

    //Open pickups
    if(problem->hasPairs()) {
        new_label->ensureOpen(problem->getNumNodes());
        if(problem->isPickup(next_node))
            new_label->setOpen(next_node);
        else if(problem->getPickupOf(next_node) >= 0)
            new_label->clearOpen(problem->getPickupOf(next_node));
    }
}

void LMDefault::updateUnreachables(LabelAdv *candidate) {
    if(use_visited) {
        int predecessor = candidate->getPredecessorNode();
        short label_type = PARTIALLY_DOMINANT;
        int node = candidate->getNode();
        bool direction = candidate->getDirection();
        std::vector<int> & neighbors = problem->getNeighbors(node, direction);
        for(auto & neigh: neighbors)
            if(candidate->isReachable(neigh) and not isExtensionFeasible(candidate, neigh, SCOPE_MONOTONE))
                candidate->setUnreachable(neigh);

        if (not isExtensionFeasible(candidate, predecessor))
            label_type = DOMINANT;

        candidate->setLabelType(label_type);
    }
}

//Insertion
//Objective based insert

LabelAdv* LMDefault::insert(LabelAdv* new_label) {
    new_label->getDirection() ? ++ins_attempts_fw : ++ins_attempts_bw;

    DataCollector* collector_label = new_label->getDirection() ? & collector_label_fw : & collector_label_bw;
    if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored()){
        collectLabel(collector_label, new_label);
        collector_label->markLastRecord(true);
    }

    //Checks if new label is suboptimal
    if(not problem->isGraphCyclic() and new_label->getObjective() > incumbent)
        return nullptr;

    const int origin = problem->getOrigin();
    const int destination = problem->getDestination();
    const int node = new_label->getNode();
    const bool direction = new_label->getDirection();

    const int objective = new_label->getObjective();

    auto & labels = direction ? forward_labels : backward_labels;
    auto & candidates = direction ? forward_candidates : backward_candidates;
    auto & best = direction ? forward_best : backward_best;
    auto & closed = direction ? forward_closed : backward_closed;
    auto & ndominated = direction ? ndominated_fw : ndominated_bw;
    auto & nclosed = direction ? nclosed_fw : nclosed_bw;

    LabelAdv* old_label;

    //Heuristic) Check if the queue is full
    if(candidates[node].size() >= queue_limit) {
        auto & c = candidates[node].back();
        old_label = getLabel(direction, c.second);

        if (old_label->getObjective() < objective or
            (old_label->getObjective() == objective and
                old_label->getSnapshot(RES_CRITICAL) <= new_label->getSnapshot(RES_CRITICAL)))
            return nullptr;

        candidates[node].pop_back();
        ndominated++;
    }

    //1) Closed labels dominate new label?
    auto cl = closed[node].begin();
    while (cl != closed[node].end()) {
        old_label = getLabel(direction, cl->second);
        if (dominates(old_label, new_label))
            return nullptr;
        if(dominates(new_label, old_label)) {
            if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored())
                collector_label->markSubsetRecord(cl->second, true);
            old_label->setDominated();
            cl = closed[node].erase(cl);
            ndominated++;
            nclosed--;
        }
        else ++cl;
    }

    //2) Open labels (with better obj) dominate new label?
    auto c = candidates[node].begin();

    while (c != candidates[node].end() and c->first < objective) {
        old_label = getLabel(direction, c->second);
        if(dominates(old_label, new_label))
            return nullptr;
        ++c;
    }

    //3) Open labels (with equal obj) dominate new label? Are they dominated by new label?
    while (c != candidates[node].end() and c->first == objective) {
        old_label = getLabel(direction, c->second);
        if (dominates(old_label, new_label))
            return nullptr;
        if (dominates(new_label, old_label)) {
            if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored())
                collector_label->markSubsetRecord(c->second, true);
            c = candidates[node].erase(c);
            ndominated++;
        }
        else ++c;
    }

    //4) Insert label
    labels.push_back(*new_label);
    int position = labels.size() - 1;
    new_label = &labels[position];

    if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored()) {
        //collectLabel(collector_label, new_label);
        collector_label->markLastRecord(false);
        collector_label->addLastRecordToSubset();
    }

    //Keep track of the lowest cost label at each node
    if(not best[node] or best[node]->getObjective() >= objective)
        best[node] = new_label;

    //Add it to either closed or open labels
    if ((direction and node == destination) or (not direction and node == origin)){
        closed[node].emplace_back(std::make_pair(objective, position));
        nclosed++;
    }
    else {
        c = candidates[node].insert(c, std::make_pair(objective, position));
        ++c;
    }
    //5) Are open labels (with worse obj) dominated by new label?
    while(c != candidates[node].end()){
        old_label = getLabel(direction, c->second);
        if(dominates(new_label, old_label)) {
            if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored())
                collector_label->markSubsetRecord(c->second, true);
            c = candidates[node].erase(c);
            ndominated++;
        }
        else ++c;
    }

    //Update incumbent
    if((node == origin or node == destination) and objective <= incumbent) {
        incumbent = objective;
        od_label.first = direction;
        od_label.second = position;
    }

    return new_label;
}

//Returns true if l1 dominates l2
bool LMDefault::dominates(LabelAdv* l1, LabelAdv* l2)  {

    if(l1->getObjective() > l2->getObjective())
        return false;

    for(int id = 0; id < l1->getSnapshot().size(); id++)
        if(l1->getSnapshot(id) > l2->getSnapshot(id))
            return false;

    //Open pickups: l1 must not carry more open requests than l2
    if(problem->hasPairs() and not is_subset(l1->getOpen(), l2->getOpen()))
        return false;

    if(not compare_unreachables)
        return true;

    if (l1->getUnreachableCount() > l2->getUnreachableCount())
        return false;

    //Unreachable dominance
    if (not is_subset(l1->getUnreachable(), l2->getUnreachable()))
        return false;

    if (not two_cycle_elimination)
        return true;

    //if L1 is dominant, L2 can be discarded
    if (l1->getLabelType() == DOMINANT)
        return true;

    //if L1 is dominated, L2 can be discarded (i.e., a partially dominant label exists that also dominates L2)
    if (l1->getLabelType() == DOMINATED)
        return l1->getExtensionTarget() != l2->getPredecessorNode();

    //Otherwise, L1 is partially dominant
    int l1_predecessor = l1->getPredecessorNode();
    int l2_predecessor = l2->getPredecessorNode();

    //if L1 and L2 have the same predecessor, L2 can be discarded
    if (l1_predecessor==l2_predecessor)
        return true;

    //if L2 is dominated and will not extend towards L1 predecessor, L2 can be discarded
    //Otherwise, L2 cannot be discarded
    if(l2->getLabelType() == DOMINATED)
        return l2->getExtensionTarget() != l1_predecessor;

    //if L2 does not have resources to reach L1 predecessor, then L2 can be discarded
    if (not isExtensionFeasible(l2, l1_predecessor))
        return true;

    //Otherwise, L2 is dominated but not discarded
    //L2 will only extend towards l1 predecessor
    l2->setLabelType(DOMINATED);
    l2->setExtensionTarget(l1_predecessor);
    return false;
}

/** Join **/
void LMDefault::join(){
    if(Parameters::getVerbosity() >= 4)
        std::cout<<"Joining..."<<std::endl;

    if(candidatesAvailable())
        closeLabels();

    switch(join_algo){
        case JOIN_NAIVE:
            naiveJoin();
            break;
        case JOIN_CLASSIC:
            classicJoin();
            break;
        case JOIN_ORDERED:
            orderedJoin();
            break;
        case JOIN_PARETO_ARC:
            paretoArcJoin();
            break;
        case JOIN_PARETO_NODE:
            paretoNodeJoin();
            break;
        case JOIN_ORDERED_NODE:
            orderedNodeJoin();
            break;
        case JOIN_KORDERED:
            ksol_orderedJoin();
            break;
        case JOIN_KORDERED_NODE:
            ksol_orderedNodeJoin();
            break;

        default:
            if (join_type == JOIN_NODE) orderedNodeJoin();
            else orderedJoin();
            break;
    }

    if(candidatesAvailable())
        restoreClosedLabels();
}


void LMDefault::closeLabels() {
    if(Parameters::getVerbosity() >= 0)
        std::cout<<"Closing open labels to perform join operations..."<<std::endl;

    forward_closed_backup = forward_closed;
    backward_closed_backup = backward_closed;
    for(int p = 0; p < forward_candidates.size(); p++){
        if(not forward_candidates[p].empty())
            for(auto & f : forward_candidates[p]) {
                forward_closed[p].push_back(f);
                nclosed_fw++;
            }
        if(not backward_candidates[p].empty())
            for(auto & b: backward_candidates[p]) {
                backward_closed[p].push_back(b);
                nclosed_bw++;
            }
    }
}

void LMDefault::restoreClosedLabels(){
    forward_closed = forward_closed_backup;
    backward_closed = backward_closed_backup;
}

void LMDefault::naiveJoin(){
    auto objective = problem->getObj();
    int cost;
    LabelAdv *label_forward, *label_backward;
    joinComparisons = 0;

    for (int i = 0; i < forward_closed.size(); i++)
        if(i != problem->getDestination())
            for(int j = 0; j < backward_closed.size(); j++)
                if(j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    for (auto &forward_data: forward_closed[i]){
                        label_forward = &forward_labels[forward_data.second];
                        for (auto &backward_data: backward_closed[j]) {
                            joinComparisons++;
                            label_backward = &backward_labels[backward_data.second];
                            cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), i, j);
                            if(cost <= incumbent and
                               isJoinFeasible(label_forward, label_backward)) {
                                incumbent = cost;
                                joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                            }
                        }
                    }
                }
}

void LMDefault::classicJoin() {
    auto objective = problem->getObj();
    int cost;
    LabelAdv *label_forward , *best_label_forward;
    LabelAdv *label_backward, *best_label_backward;
    joinComparisons = 0;

    for (int i = 0; i < forward_closed.size(); i++)
        if (forward_best[i] and i != problem->getDestination())
            for (int j = 0; j < backward_closed.size(); j++)
                if ((backward_best[j]) and j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    cost = objective->join(forward_best[i]->getObjective(), backward_best[j]->getObjective(), i, j);
                    //cost best f(i) + best b(j) > bound -> go to next node j
                    if (cost <= incumbent) {
                        for (auto &forward_data: forward_closed[i]) {
                            label_forward = &forward_labels[forward_data.second];
                            cost = objective->join(label_forward->getObjective(), backward_best[j]->getObjective(), i, j);
                            //cost f(i) + best b(j) > bound -> go to next label f(i)
                            if (cost <= incumbent) {
                                for (auto &backward_data: backward_closed[j]) {
                                    joinComparisons++;
                                    label_backward = &backward_labels[backward_data.second];
                                    cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), i, j);
                                    if (cost <= incumbent and
                                        isJoinFeasible(label_forward, label_backward)) {
                                        incumbent = cost;
                                        joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                                    }
                                }
                            }
                        }
                    }
                }
}

void LMDefault::orderedJoin(){
    auto objective = problem->getObj();
    std::multiset<std::tuple<int, int, int>> orderedPairs;
    int cost;
    int i, j;

    //Sort closed
    for(int p = 0; p < forward_closed.size(); p++){
        if(not forward_closed[p].empty())
            std::sort(forward_closed[p].begin(), forward_closed[p].end());
        if(not backward_closed[p].empty())
            std::sort(backward_closed[p].begin(), backward_closed[p].end());
    }

    //Prepare data structure
    for(i = 0; i < forward_closed.size(); i++)
        if(not forward_closed[i].empty() and i != problem->getDestination())
            for(j = 0; j < backward_closed.size(); j++)
                if(not backward_closed[j].empty() and j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    cost = objective->join(forward_best[i]->getObjective(), backward_best[j]->getObjective(), i, j);
                    if(cost <= incumbent)
                        orderedPairs.insert(std::make_tuple(cost, i, j));
                }

    //attempt join
    joinComparisons = 0;
    LabelAdv *label_forward, *label_backward;
    while(not orderedPairs.empty()){
        cost = std::get<0>(*orderedPairs.begin());
        if(cost <= incumbent){
            i = std::get<1>(*orderedPairs.begin());
            j = std::get<2>(*orderedPairs.begin());
            orderedPairs.erase(orderedPairs.begin());
            for(auto & f: forward_closed[i]) {
                label_forward = &forward_labels[f.second];
                //If current label (i) +  best label of j is worse than bound, skip to next i-j combination
                if(objective->join(label_forward->getObjective(), backward_best[j]->getObjective(), i, j) <= incumbent) {
                    for (auto &b: backward_closed[j]){
                        label_backward = &backward_labels[b.second];
                        cost = objective->join(label_forward->getObjective(), label_backward->getObjective() , i, j);
                        if (cost <= incumbent){
                            joinComparisons++;
                            if(isJoinFeasible(label_forward, label_backward)) {
                                incumbent = cost;
                                joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                                break;
                            }
                        }
                    }
                }
                else break;
            }
        }
        else break;
    }
}


void LMDefault::ksol_orderedJoin(){
    auto objective = problem->getObj();
    std::multiset<std::tuple<int, int, int>> orderedPairs;
    int kth_cost = UNKNOWN;
    int cost;
    int i, j;

    //Sort closed
    for(int p = 0; p < forward_closed.size(); p++){
        if(not forward_closed[p].empty())
            std::sort(forward_closed[p].begin(), forward_closed[p].end());
        if(not backward_closed[p].empty())
            std::sort(backward_closed[p].begin(), backward_closed[p].end());
    }

    //Prepare data structure
    for(i = 0; i < forward_closed.size(); i++)
        if(not forward_closed[i].empty() and i != problem->getDestination())
            for(j = 0; j < backward_closed.size(); j++)
                if(not backward_closed[j].empty() and j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    cost = objective->join(forward_best[i]->getObjective(), backward_best[j]->getObjective(), i, j);
                    orderedPairs.insert(std::make_tuple(cost, i, j));
                }

    //attempt join
    joinComparisons = 0;
    LabelAdv *label_forward, *label_backward;
    while(not orderedPairs.empty()){
        cost = std::get<0>(*orderedPairs.begin());
        if(cost > kth_cost)
            break;
        i = std::get<1>(*orderedPairs.begin());
        j = std::get<2>(*orderedPairs.begin());
        orderedPairs.erase(orderedPairs.begin());

        for(auto & f: forward_closed[i]) {
            label_forward = &forward_labels[f.second];
            //If current label (i) + best label of j is worse than bound, skip to next i-j combination
            if(objective->join(label_forward->getObjective(), backward_best[j]->getObjective(), i, j) > kth_cost)
                break;

            for(auto & b: backward_closed[j]) {
                label_backward = &backward_labels[b.second];
                cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), i, j);
                if(cost > kth_cost)
                    break;

                joinComparisons++;
                if(isJoinFeasible(label_forward, label_backward)) {
                    if(cost < incumbent) incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                    if(joinable_labels.size() > requested_solutions) {
                        joinable_labels.erase(std::prev(joinable_labels.end()));
                        kth_cost = std::get<0>(*std::prev(joinable_labels.end()));
                    }
                }
            }
        }
    }
}

void LMDefault::ksol_orderedNodeJoin(){
    auto objective = problem->getObj();
    int kth_cost = UNKNOWN;
    joinComparisons = 0;
    LabelAdv *label_forward, *label_backward;

    for(int node = 0; node < forward_closed.size(); node++){
        auto & fw = forward_closed[node];
        auto & bw = backward_closed[node];

        if(fw.empty() or bw.empty())
            continue;

        int best_cost = objective->join(forward_best[node]->getObjective(), backward_best[node]->getObjective(), node);
        if(best_cost > kth_cost)
            continue;

        std::sort(fw.begin(), fw.end());
        std::sort(bw.begin(), bw.end());

        for(int i = 0; i < fw.size(); i++){
            label_forward = &forward_labels[fw[i].second];
            best_cost = objective->join(label_forward->getObjective(), backward_best[node]->getObjective(), node);
            //If current label + best backward is worse than bound, skip remaining forward labels
            if(best_cost > kth_cost)
                break;

            for(int j = 0; j < bw.size(); j++){
                label_backward = &backward_labels[bw[j].second];
                int cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), node);
                if(cost > kth_cost)
                    break;

                joinComparisons++;
                if(isNodeJoinFeasible(label_forward, label_backward)) {
                    if(cost < incumbent) incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                    if(joinable_labels.size() > requested_solutions) {
                        joinable_labels.erase(std::prev(joinable_labels.end()));
                        kth_cost = std::get<0>(*std::prev(joinable_labels.end()));
                    }
                }
            }
        }
    }
}

void LMDefault::orderedNodeJoin() {
    auto objective = problem->getObj();
    joinComparisons = 0;
    LabelAdv *label_forward, *label_backward;

    for (int node = 0; node < forward_closed.size(); node++){
        auto & fw = forward_closed[node];
        auto & bw = backward_closed[node];

        if (fw.empty() or bw.empty())
            continue;

        int best_cost = objective->join(forward_best[node]->getObjective(), backward_best[node]->getObjective(), node);

        if (best_cost > incumbent)
            continue;

        std::sort(fw.begin(), fw.end());
        std::sort(bw.begin(), bw.end());

        for (int i = 0; i < fw.size(); i++){
            label_forward = &forward_labels[fw[i].second];
            best_cost = objective->join(label_forward->getObjective(), backward_best[node]->getObjective(), node);
            if (best_cost > incumbent)
                break;

            for (int j = 0; j < bw.size(); j++){
                label_backward = &backward_labels[bw[j].second];

                int cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), node);
                if (cost > incumbent)
                    break;

                joinComparisons++;
                if(isNodeJoinFeasible(label_forward, label_backward)) {
                    incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                    break;
                }
            }
        }
    }
}

// ParetoJoin
// Binary-search based Pareto pruning: exploits sorted (rows, cols) to discard whole
// blocks of dominated pairs without enumerating them, converging on the best feasible join.
// rows/cols represent the forward/backward label lists; start/end_row/col bound the sub-block explored.
void LMDefault::paretoSearchBlock(Resource* objective, int node_fw, int node_bw,
                                   std::vector<std::pair<int, int>>& rows,
                                   std::vector<std::pair<int, int>>& cols,
                                   int start_row, int end_row, int start_col, int end_col) {

    // Join cost and feasibility for a (fw, bw) pair on arc or node
    auto joinCost = [&](LabelAdv* fw, LabelAdv* bw) {
        return (node_fw == node_bw)
            ? objective->join(fw->getObjective(), bw->getObjective(), node_fw)
            : objective->join(fw->getObjective(), bw->getObjective(), node_fw, node_bw);
    };
    auto joinFeasible = [&](LabelAdv* fw, LabelAdv* bw) {
        return (node_fw == node_bw)
            ? isNodeJoinFeasible(fw, bw)
            : isJoinFeasible(fw, bw);
    };

    // Check: Best solution
    // (rows[start_row], cols[start_col]) is the cheapest pair in this block.
    // If its cost already meets or exceeds the incumbent the entire block is suboptimal.
    LabelAdv* fw_label = &forward_labels [rows[start_row].second];
    LabelAdv* bw_label = &backward_labels[cols[start_col].second];
    int cost           = joinCost(fw_label, bw_label);
    if (cost >= incumbent)
        return;

    // Base a: Single cell
    // cost < incumbent is already guaranteed from other steps; only feasibility is open.
    if (start_row == end_row && start_col == end_col) {
        joinComparisons++;
        if (joinFeasible(fw_label, bw_label)) {
            incumbent = cost;
            best_fw   = fw_label;
            best_bw   = bw_label;
        }
        return;
    }

    // Base b: Single row (one forward label, several backward)
    // Scan backward labels left-to-right (increasing cost).
    // The first feasible join is optimal for this row.
    // The first pair with cost ≥ incumbent means all remaining are suboptimal.
    if (start_row == end_row) {
        joinComparisons++;
        if (joinFeasible(fw_label, bw_label)) {
            incumbent = cost;
            best_fw   = fw_label;
            best_bw   = bw_label;
            return;
        }
        for (int col = start_col + 1; col <= end_col; col++) {
            bw_label = &backward_labels[cols[col].second];
            cost     = joinCost(fw_label, bw_label);
            if (cost >= incumbent) return;
            joinComparisons++;
            if (joinFeasible(fw_label, bw_label)) {
                incumbent = cost;
                best_fw   = fw_label;
                best_bw   = bw_label;
                return;
            }
        }
        return;
    }

    // Base c: Single column (one backward label, several forward)
    // Scan forward labels top-to-bottom (increasing cost), same logic as single row.
    if (start_col == end_col) {
        joinComparisons++;
        if (joinFeasible(fw_label, bw_label)) {
            incumbent = cost;
            best_fw   = fw_label;
            best_bw   = bw_label;
            return;
        }
        for (int row = start_row + 1; row <= end_row; row++) {
            fw_label = &forward_labels[rows[row].second];
            cost     = joinCost(fw_label, bw_label);
            if (cost >= incumbent) return;
            joinComparisons++;
            if (joinFeasible(fw_label, bw_label)) {
                incumbent = cost;
                best_fw   = fw_label;
                best_bw   = bw_label;
                return;
            }
        }
        return;
    }

    // Step 1: Binary search
    // win_* tracks the search window; it starts as the full block and shrinks
    // until it collapses to a single cell (the pivot, handled in Step 2).
    int win_row_start = start_row, win_row_end = end_row;
    int win_col_start = start_col, win_col_end = end_col;

    while (win_row_start < win_row_end || win_col_start < win_col_end) {
        int mid_row = (win_row_start + win_row_end) / 2;
        int mid_col = (win_col_start + win_col_end) / 2;

        fw_label = &forward_labels [rows[mid_row].second];
        bw_label = &backward_labels[cols[mid_col].second];
        cost     = joinCost(fw_label, bw_label);

        if (cost >= incumbent) {
            // Midpoint is suboptimal: everything to its bottom-right is also suboptimal.
            // Shrink the window toward top-left, keeping each axis fixed if already a single point.
            if (win_row_start != win_row_end) win_row_end = mid_row - 1;
            if (win_col_start != win_col_end) win_col_end = mid_col - 1;
        } else {
            joinComparisons++;
            if (joinFeasible(fw_label, bw_label)) {
                // Feasible improvement: update incumbent.
                // The new incumbent makes the bottom-right suboptimal → same shrink as above.
                incumbent = cost;
                best_fw   = fw_label;
                best_bw   = bw_label;
                if (win_row_start != win_row_end) win_row_end = mid_row - 1;
                if (win_col_start != win_col_end) win_col_end = mid_col - 1;
            } else {
                // Infeasible: cannot prune bottom-right, must explore it.
                // Shift the window lower-right.
                if (win_row_start != win_row_end) win_row_start = mid_row + 1;
                if (win_col_start != win_col_end) win_col_start = mid_col + 1;
            }
        }
    }

    // Step 2: Pivot test
    // The search window has collapsed to a single cell: the pivot.
    // We test it and use the result to decide how to split the remaining unexplored block.
    const int pivot_row = win_row_start;  // == win_row_end after convergence
    const int pivot_col = win_col_start;  // == win_col_end after convergence

    fw_label = &forward_labels [rows[pivot_row].second];
    bw_label = &backward_labels[cols[pivot_col].second];
    cost     = joinCost(fw_label, bw_label);

    // prune_bottom_right is true when the pivot is suboptimal OR feasible:
    //   in both cases the bottom-right quadrant of the pivot can be discarded.
    // It is false only when the pivot is infeasible with cost < incumbent,
    //   meaning bottom-right pairs might still be feasible.
    bool prune_bottom_right;
    if (cost >= incumbent) {
        prune_bottom_right = true;
    } else {
        joinComparisons++;
        if (joinFeasible(fw_label, bw_label)) {
            incumbent          = cost;
            best_fw            = fw_label;
            best_bw            = bw_label;
            prune_bottom_right = true;
        } else {
            prune_bottom_right = false;
        }
    }

    // Step 3: Recursive calls
    // Recursion is only meaningful when both dimensions are non-trivial at the
    // outer level (single-row / single-col cases are fully handled in Base 1).
    if (start_row >= end_row or start_col >= end_col)
        return;

    if (prune_bottom_right) {
        // The pivot (and everything to its bottom-right) is suboptimal or already
        // represented by the updated incumbent.  Two unexplored regions remain:
        //   Top strip : rows strictly above the pivot, all columns.
        //   Left strip: rows from pivot downward, columns strictly left of pivot.
        if (start_row < pivot_row)
            paretoSearchBlock(objective, node_fw, node_bw, rows, cols,
                              start_row, pivot_row - 1, start_col, end_col);
        if (start_col < pivot_col)
            paretoSearchBlock(objective, node_fw, node_bw, rows, cols,
                              pivot_row, end_row, start_col, pivot_col - 1);
    } else {
        // Pivot is infeasible: higher-cost label pairs may still be resource-compatible,
        // so the bottom-right cannot be discarded entirely.
        if (pivot_row == end_row) {
            // Pivot is on the last row of the block.
            // Explore all rows above it (with all columns), then the columns to its
            // left on the pivot row itself.
            if (start_row < pivot_row)
                paretoSearchBlock(objective, node_fw, node_bw, rows, cols,
                                  start_row, pivot_row - 1, start_col, end_col);
            if (start_col < pivot_col)      
                paretoSearchBlock(objective, node_fw, node_bw, rows, cols,
                                  pivot_row, pivot_row, start_col, pivot_col - 1);
        } else {
            // Pivot is not on the last row: explore all rows up to and including
            // the pivot row (the pivot itself was infeasible but columns to its left
            // are still unexplored on that row, handled by the recursive call).
            paretoSearchBlock(objective, node_fw, node_bw, rows, cols,
                              start_row, pivot_row, start_col, end_col);
        }
        // Explore the rows below the pivot with columns up to pivot_col.
        if (pivot_row < end_row)
            paretoSearchBlock(objective, node_fw, node_bw, rows, cols,
                              pivot_row + 1, end_row, start_col, pivot_col);
    }
}

// Arc-join entry point for the Pareto procedure.
// For each arc (i → j) in the network, calls paretoSearchBlock to find the best
// feasible combination of a forward label ending at i and a backward label starting at j.
// Label sets are sorted by non-decreasing cost before the search (required invariant).
// The overall best join found is stored in joinable_labels.
void LMDefault::paretoArcJoin() {
    incumbent       = UNKNOWN; 
    joinComparisons = 0;
    best_fw         = nullptr;
    best_bw         = nullptr;

    // Sort each closed label list by cost (non-decreasing) .
    for (int node = 0; node < (int)forward_closed.size(); node++) {
        if (!forward_closed[node].empty())
            std::sort(forward_closed[node].begin(), forward_closed[node].end());
        if (!backward_closed[node].empty())
            std::sort(backward_closed[node].begin(), backward_closed[node].end());
    }

    auto* objective = problem->getObj();

    for (int i = 0; i < (int)forward_closed.size(); i++) {
        // Skip: no forward labels at i, or i is the destination (complete paths, not candidates).
        if (not forward_best[i] or i == problem->getDestination())
            continue;

        for (int j = 0; j < (int)backward_closed.size(); j++) {
            // Skip: no backward labels at j, j is the origin, or (i,j) is not an arc.
            if (not backward_best[j] or j == problem->getOrigin() or not problem->areNeighbors(i, j, true))
                continue;

            paretoSearchBlock(objective, i, j,
                              forward_closed[i],  backward_closed[j],
                              0, (int)forward_closed[i].size()  - 1,
                              0, (int)backward_closed[j].size() - 1);
        }
    }

    if (best_fw && best_bw)
        joinable_labels.insert(std::make_tuple(incumbent, best_fw, best_bw));
}

// Node-join entry point for the Pareto procedure.
// For each node, calls paretoSearchBlock to find the best feasible combination of a
// forward label and a backward label both closed at that same node.
// Label sets are sorted by non-decreasing cost before the search (required invariant).
// The overall best join found is stored in joinable_labels.
void LMDefault::paretoNodeJoin() {
    incumbent       = UNKNOWN;
    joinComparisons = 0;
    best_fw         = nullptr;
    best_bw         = nullptr;

    // Sort each closed label list by cost (non-decreasing) .
    for (int node = 0; node < (int)forward_closed.size(); node++) {
        if (!forward_closed[node].empty())
            std::sort(forward_closed[node].begin(), forward_closed[node].end());
        if (!backward_closed[node].empty())
            std::sort(backward_closed[node].begin(), backward_closed[node].end());
    }

    auto* objective = problem->getObj();

    for (int node = 0; node < (int)forward_closed.size(); node++) {
        // Skip: no forward/backward labels closed at this node, or it's the origin/destination.
        if (not forward_best[node] or not backward_best[node]
            or node == problem->getOrigin() or node == problem->getDestination())
            continue;

        paretoSearchBlock(objective, node, node,
                          forward_closed[node], backward_closed[node],
                          0, (int)forward_closed[node].size()  - 1,
                          0, (int)backward_closed[node].size() - 1);
    }

    if (best_fw && best_bw)
        joinable_labels.insert(std::make_tuple(incumbent, best_fw, best_bw));
}

bool LMDefault::isJoinFeasible(LabelAdv* label_forward, LabelAdv* label_backward) {
    int i = label_forward->getNode();
    int j = label_backward->getNode();
    int snapshot_forward, snapshot_backward;
    std::vector<Resource*>& resources = problem->getResources();

    if (two_cycle_elimination){
        int i_predecessor = label_forward->getPredecessorNode();
        int j_predecessor = label_backward->getPredecessorNode();
        if (i == j_predecessor or j == i_predecessor)
            return false;
    }

    for(int resID = 0; resID < problem->getNumRes(); resID++) {
        snapshot_forward = label_forward->getSnapshot(resID);
        snapshot_backward = label_backward->getSnapshot(resID);
        int current_value = resources[resID]->join(snapshot_forward, snapshot_backward, i, j);
        if(not resources[resID]->isFeasible(current_value))
            return false;
    }

    if(use_visited)
        return (label_forward->getVisited() & label_backward->getVisited()).none();

    return true;
}

bool LMDefault::isNodeJoinFeasible(LabelAdv* label_forward, LabelAdv* label_backward) {
    int node = label_forward->getNode();

    int snapshot_forward, snapshot_backward;
    std::vector<Resource*>& resources = problem->getResources();

    if (two_cycle_elimination) {
        int i_predecessor = label_forward->getPredecessorNode();
        int j_predecessor = label_backward->getPredecessorNode();
        if (j_predecessor == i_predecessor)
            return false;
    }

    for(int resID = 0; resID < problem->getNumRes(); resID++) {
        snapshot_forward = label_forward->getSnapshot(resID);
        snapshot_backward = label_backward->getSnapshot(resID);
        int current_value = resources[resID]->join(snapshot_forward, snapshot_backward, node);
        if(!resources[resID]->isFeasible(current_value))
            return false;
    }

    if(use_visited) {
        auto mask = label_forward->getVisited() & label_backward->getVisited();
        mask.reset(node);
        return mask.none();
    }
    return true;
}



/** Solution management +*/
//Return a solution
std::tuple<int, LabelAdv*, LabelAdv*> LMDefault::getSolutionLabels() {
    std::tuple<int, LabelAdv*, LabelAdv*> solution_data = {0, nullptr, nullptr};

    if(bidirectional and joinFound())
        solution_data = getBestJoin();
    else {
        LabelAdv* candidate = getODLabel();
        if(candidate){
            std::get<0>(solution_data) = candidate->getObjective();
            candidate->getDirection()? std::get<1>(solution_data) = candidate : std::get<2>(solution_data) = candidate;
        }
    }

    return solution_data;
}

void LMDefault::sortClosedLabels(bool direction, int node) {
    auto& closed = direction ? forward_closed[node] : backward_closed[node];
    std::sort(closed.begin(), closed.end());
}

const std::vector<std::pair<int, int>>& LMDefault::getClosedLabels(bool direction, int node) {
    return direction ? forward_closed[node] : backward_closed[node];
}

void LMDefault::setODLabel() {
    int bound = UNKNOWN;
    int index = - 1;
    bool direction = true;

    for(auto & candidate: forward_closed[problem->getDestination()])
        if(forward_labels[candidate.second].getObjective() < bound) {
            bound = candidate.first;
            index = candidate.second;
        }
    for(auto & candidate: backward_closed[problem->getOrigin()])
        if(backward_labels[candidate.second].getObjective() < bound) {
            bound = candidate.first;
            index = candidate.second;
            direction = false;
        }

    if(index >= 0) {
        od_label.first = direction;
        od_label.second = index;
    }
}

/** Debug **/
//Finds labels with same obj and resource consumption
std::list<LabelAdv> LMDefault::buildTour(std::list<int> tour, bool direction) {
    if(!direction) tour.reverse();

    std::list<LabelAdv> tourLabels;

    tourLabels.emplace_back();
    LabelAdv* current_label = & tourLabels.front();

    int node = tour.front();
    tour.pop_front();

    current_label->initLabel(node, nullptr, direction, problem->getNumRes());
    current_label->setObjective(problem->getObj()->getInitValue() + problem->getObj()->getNodeCost(node));

    std::vector<Resource*>& resources = problem->getResources();
    for(int id = 0; id < problem->getNumRes(); id++)
        current_label->setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(node));

    for(auto node: tour) {
        LabelAdv new_label;
        extendLabel(current_label, &new_label, node);
        tourLabels.push_back(new_label);
        current_label = & tourLabels.back();
    }

    return tourLabels;
}

void LMDefault::findLabels(std::list<LabelAdv> *tourLabels) {
    if(Parameters::getVerbosity() < 0)
        return;

    int position;
    bool direction;
    bool found;
    LabelAdv* l_stored;
    std::vector<std::vector<std::pair<int, int>>> * stored_data;

    for(auto & l_current: *tourLabels) {
        position = l_current.getNode();
        direction = l_current.getDirection();
        found = false;

        if(direction) stored_data = & forward_closed;
        else stored_data = & backward_closed;

        for(auto & label_data: (*stored_data)[position]) {
            int index = std::get<1>(label_data);

            if(direction) l_stored = & forward_labels[index];
            else l_stored = & backward_labels[index];

            if(l_current.getObjective() == l_stored->getObjective()
               and l_current.getSnapshot() == l_stored->getSnapshot()){
                found = true;
                break;
            }

        }

        if(found)
            std::cout<<"Debug: Every label was found." << std::endl;
        else
            std::cout<<"Debug: Could not find matching label at node."  << position << std::endl;
    }
}

void LMDefault::printStepConsumption(std::list<LabelAdv> *tourFW, std::list<LabelAdv> *tourBW) {
    if(Parameters::getVerbosity() < 0)
        return;

    std::vector<std::vector<int>> consumptionsFW, consumptionsBW;
    consumptionsFW.resize(problem->getNumRes(), std::vector<int>());
    consumptionsBW.resize(problem->getNumRes(), std::vector<int>());

    for(auto & l: *tourFW)
        for(int r = 0; r < problem->getNumRes(); r++)
            consumptionsFW[r].push_back(l.getSnapshot(r));

    if(tourBW)
        for(auto & l: *tourBW)
            for(int r = 0; r < problem->getNumRes(); r++)
                consumptionsBW[r].push_back(l.getSnapshot(r));


    std::cout<<"Resource consumption at each node of the tour"<<std::endl;
    for(int r = 0; r < problem->getNumRes(); r++) {
        std::cout<<"Resource " << r << ": " << problem->getRes(r)->getName() << std::endl;
        std::cout<< "Direction: forward" << std::endl;
        for(int t = 0; t < tourFW->size(); t++)
            std::cout<< consumptionsFW[r][t]<< " ";
        std::cout<<std::endl;

        if(tourBW) {
            std::cout<< "Direction: backward" << std::endl;
            for(int t = 0; t < tourBW->size(); t++)
                std::cout<<consumptionsBW[r][t]<< " ";
            std::cout<<std::endl;
        }

        std::cout<<"-------"<<std::endl;
    }

}

void LMDefault::printCandidates(int id, bool direction) {
    if(Parameters::getVerbosity() < 0)
        return;

    std::cout<<"Printing all candidate labels for node " << id << " with direction " << direction << std::endl;

    LabelAdv* label;
    auto candidates = direction ? forward_candidates[id] : backward_candidates[id];
    for(auto c: candidates)
        direction ? forward_labels[c.second].printLabel() : backward_labels[c.second].printLabel();
}

void LMDefault::printClosed(int id, bool direction) {
    if(Parameters::getVerbosity() < 0)
        return;

    std::cout<<"Printing all closed labels for node " << id << " with direction " << direction << std::endl;

    LabelAdv* label;
    auto closed = direction ? forward_closed[id] : backward_closed [id];

    for(auto c: closed)
        direction ? forward_labels[c.second].printLabel() : backward_labels[c.second].printLabel();
}

/** Data collection management **/

void LMDefault::initDataCollection() {
    if(not Parameters::isCollecting())
        return;

    collector.init("lm_name", name);
    collector.init("data_structure", lm_type);
    collector.init("executionID", 0);
    collector.init("compare_unreachables", 0);
    collector.init("use_visited", 0);
    collector.init("candidate_type", "-1");
    collector.init("join_algo", "-1");
    collector.init("iterations", 0);
    collector.init("ins_attempts_fw", 0);
    collector.init("ins_attempts_bw", 0);
    collector.init("nlabels_inserted", 0);
    collector.init("nfw_inserted", 0);
    collector.init("nbw_inserted", 0);
    collector.init("ndominated", 0);
    collector.init("ndominated_fw", 0);
    collector.init("ndominated_bw", 0);
    collector.init("nclosed", 0);
    collector.init("nclosed_fw", 0);
    collector.init("nclosed_bw", 0);
    collector.init("njoin", "0");
    collector.init("split", split_ratio);
    collector.setHeader();

    initLabelDataCollection(&collector_label_fw);
    initLabelDataCollection(&collector_label_bw);
}
void LMDefault::initLabelDataCollection(DataCollector* collector_label) {
    if(Parameters::getCollectionLevel() < 4)
        return;

    //collector_label->init("executionID", 0);
    collector_label->init("iterations", 0);
    collector_label->init("direction", 0);
    collector_label->init("node", -1);
    collector_label->init("predecessor", -1);
    collector_label->init("objective", UNKNOWN);
    collector_label->init("consumption_critical", UNKNOWN);
    for(int i = 1; i < problem->getNumRes(); i++)
        collector_label->init("consumption_"+std::to_string(i), UNKNOWN);
    collector_label->init("tour_length", -1);
    collector_label->init("nvisited", -1);
    collector_label->init("nunreachable", -1);
    collector_label->init("nvisited_unaltered", -1);
    collector_label->init("repeated_visits", -1);

    //Globally
    collector_label->init("nlabels_network", -1);
    collector_label->init("nlabels_dominated_network", -1);
    collector_label->init("nlabels_closed_network", -1);
    collector_label->init("nlabels_open_network", -1);
    //At insertion node
    collector_label->init("nlabels_node", -1);
    collector_label->init("nlabels_closed_node", -1);
    collector_label->init("nlabels_open_node", -1);
    collector_label->setHeader();

}

float LMDefault::getMeanLabels(bool direction){
    float sum_size = 0;

    auto & closed_labels = direction ? forward_closed : backward_closed;
    for(auto & c: closed_labels)
        sum_size += c.size();

    return (sum_size/closed_labels.size());
}

int LMDefault::getMaxLabels(bool direction) {
    int max_size = 0;

    auto & closed_labels = direction ? forward_closed : backward_closed;
    for(auto & c: closed_labels)
        if(max_size < c.size())
            max_size = c.size();

    return max_size;
}

float LMDefault::getVarLabels(bool direction, float mean) {
    float sum_square = 0;

    auto & closed_labels = direction ? forward_closed : backward_closed;
    for(auto & c: closed_labels)
        sum_square += (mean - c.size())*(mean - c.size());

    return (sum_square/closed_labels.size());
}

void LMDefault::collectData() {
    if(not Parameters::isCollecting())
        return;

    collector.collect("lm_name", name);
    collector.collect("executionID", executionID);
    collector.collect("iterations", iterations);
    collector.collect("compare_unreachables", compare_unreachables);
    collector.collect("use_visited", use_visited);
    std::string candidate_type_st;

    switch(candidate_type){
        case CANDIDATE_RR:  candidate_type_st = "round-robin"; break;
        default: candidate_type_st = "node"; break;
    }
    collector.collect("candidate_type", candidate_type_st);

    std::string join_algo_st;

    switch(join_algo){
        case JOIN_NAIVE:  join_algo_st = "naive (arc)"; break;
        case JOIN_CLASSIC:  join_algo_st = "classic (arc)"; break;
        case JOIN_ORDERED:  join_algo_st = "ordered (arc)"; break;
        case JOIN_PARETO_ARC:  join_algo_st = "pareto (arc)"; break;
        case JOIN_ORDERED_NODE:  join_algo_st = "ordered (node)"; break;
        case JOIN_KORDERED:  join_algo_st = "kordered (arc)"; break;
        case JOIN_KORDERED_NODE:  join_algo_st = "kordered (node)"; break;
        default: join_algo_st = "default"; break;
    }
    collector.collect("join_algo", join_algo_st);

    collector.collect("ins_attempts_fw", ins_attempts_fw);
    collector.collect("ins_attempts_bw", ins_attempts_bw);
    collector.collect("nlabels_inserted", (int) (forward_labels.size() + backward_labels.size()));
    collector.collect("nfw_inserted", (int) forward_labels.size());
    collector.collect("nbw_inserted", (int) backward_labels.size());
    collector.collect("ndominated", ndominated_fw + ndominated_bw);
    collector.collect("ndominated_fw", ndominated_fw);
    collector.collect("ndominated_bw", ndominated_bw);
    collector.collect("nclosed", nclosed_fw + nclosed_bw);
    collector.collect("nclosed_fw", nclosed_fw);
    collector.collect("nclosed_bw", nclosed_bw);
    collector.collect("njoin", std::to_string(joinComparisons));
    collector.saveRecord();
}

void LMDefault::collectLabel(DataCollector* collector_label, LabelAdv *l) {
    if(Parameters::getCollectionLevel() < 4)
        return;

    bool direction = l->getDirection();
    int node = l->getNode();

    //collector_label->collect("executionID", executionID);
    collector_label->collect("iterations", iterations);
    collector_label->collect("direction", l->getDirection());
    collector_label->collect("node", node);
    collector_label->collect("predecessor", l->getPredecessorNode());
    collector_label->collect("objective", l->getObjective());
    collector_label->collect("consumption_critical", l->getSnapshot(RES_CRITICAL));
    for(int i = 1; i < problem->getNumRes(); i++)
        collector_label->collect("consumption_"+std::to_string(i), l->getSnapshot(i));

    collector_label->collect("nvisited", (int) l->getVisited().count());
    collector_label->collect("nunreachable", (int) l->getUnreachable().count());

    //Globally
    int n_open = direction? getForwardSize()  - ndominated_fw - nclosed_fw : getBackwardSize()  - ndominated_bw - nclosed_bw;
    collector_label->collect("nlabels_network", direction? getForwardSize() : getBackwardSize());
    collector_label->collect("nlabels_dominated_network", direction? ndominated_fw : ndominated_bw);
    collector_label->collect("nlabels_closed_network", direction? nclosed_fw : nclosed_bw);
    collector_label->collect("nlabels_open_network", n_open);

    //Node
    int nlabels_closed_node = direction ? forward_closed[node].size() : backward_closed[node].size();
    int nlabels_open_node = direction ? forward_candidates[node].size() : backward_candidates[node].size();
    int nlabels_node = nlabels_closed_node + nlabels_open_node;
    collector_label->collect("nlabels_node", nlabels_node);
    collector_label->collect("nlabels_closed_node", nlabels_closed_node);
    collector_label->collect("nlabels_open_node", nlabels_open_node);


    collector_label->saveRecord();
}


