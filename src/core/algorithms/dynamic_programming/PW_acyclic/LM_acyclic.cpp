#include "LM_acyclic.h"
//PW_LMAcyclic
//only for integer solutions, acyclic networks

/** LM management **/
//Constructors and destructors
LMacyclic::LMacyclic(Problem* problem): problem(problem) {
    bound_labels = problem->getBoundLabels();
    name = "label_manager";
    lm_type = "bucket";
    executionID = 0;
    initDataCollection();
}

//Init Label Manager
void LMacyclic::initLM() {
    //Init Data links
    obj = problem->getObj();
    res = problem->getRes(RES_CRITICAL);

    //Initialize Data
    int origin = problem->getOrigin();
    int destination = problem->getDestination();
    int n_nodes = problem->getNumNodes();
    int n_res = problem->getNumRes();

    //Init Node Data Structure
    closed_f.resize(n_nodes, std::list<Label*>());
    closed_b.resize(n_nodes, std::list<Label*>());
    min_consumption_f.resize(n_nodes, MAX_INT);
    min_consumption_b.resize(n_nodes, MAX_INT);

    bool direction = true;
    Label* predecessor = nullptr;

    //Creates first Forward label
    Label* originLabel = new Label();
    originLabel->initLabel(origin, predecessor, direction, n_res); //Consider a default initialization?
    originLabel->setObjective(obj->getInitValue() + obj->getNodeCost(origin));
    originLabel->setSnapshot(RES_CRITICAL, res->getInitValue() + res->getNodeCost(origin));

    //Creates first Backward label
    Label* destinationLabel = new Label();
    destinationLabel->initLabel(destination, predecessor, false, n_res);
    destinationLabel->setObjective(obj->getInitValue() + obj->getNodeCost(destination));
    destinationLabel->setSnapshot(RES_CRITICAL, res->getInitValue() + res->getNodeCost(destination));

    //Add labels ids to buckets
    cost_t bucket_range = 1;
    cost_t bucket_init_value;
    Label* cost_label;

    //Forward
    pos_f = 0;
    cost_label = bound_labels->getLabel(RES_COST, not direction, origin);
    bucket_init_value = originLabel->getObjective() + cost_label->getObjective(); //completion bound
    bucket_range = incumbent - bucket_init_value + 1; //range_f
    offset_f = bucket_init_value;
    bucket_f.resize(bucket_range, std::list<Label*>());
    bucket_f[pos_f].emplace_back(originLabel);

    //Backward
    pos_b = 0;
    cost_label = bound_labels->getLabel(RES_COST, direction, destination);
    bucket_init_value = destinationLabel->getObjective() + cost_label->getObjective();
    bucket_range = incumbent - bucket_init_value + 1;
    offset_b = bucket_init_value;
    bucket_b.resize(bucket_range, std::list<Label*>());
    bucket_b[pos_b].emplace_back(destinationLabel);

    nlabels_fw = nlabels_bw  = 1;
    joinComparisons = earlyImprov = nclosed_fw = nclosed_bw = 0;
}

void LMacyclic::resetLM(){
    if(bucket_f.empty() and bucket_b.empty())
        return;

    for(int i = pos_f; i < bucket_f.size(); i++) {
        if(not bucket_f[i].empty())
            for(auto l:  bucket_f[i])
                delete l;
    }

    for(int i = pos_b; i < bucket_b.size(); i++) {
        if(not bucket_b[i].empty())
            for(auto l:  bucket_b[i])
                delete l;
    }

    pos_f = 0;
    pos_b = 0;

    for(int i = 0; i < problem->getNumNodes(); i++) {
        if(not closed_f.empty() and not closed_f[i].empty())
            for(auto l:  closed_f[i])
                delete l;

        if(not closed_b.empty() and not closed_b[i].empty())
            for(auto l:  closed_b[i])
                delete l;
    }
}

//Support methods
//Close labels
void LMacyclic::closeLabel(Label *current) {
    int node = current->getNode();
    bool direction = current->getDirection();
    direction ? nclosed_fw++ : nclosed_bw++;
    auto & closed = direction ? closed_f[node] : closed_b[node];
    closed.push_back(current);
}

//Builds a tour
std::list<Label> LMacyclic::buildTour(std::list<int> tour, bool direction) {
    if(!direction) tour.reverse();

    std::list<Label> tourLabels;

    tourLabels.emplace_back();
    Label* current_label = & tourLabels.front();

    int node = tour.front();
    tour.pop_front();

    current_label->initLabel(node, nullptr, direction, problem->getNumRes());

    current_label->setObjective(problem->getObj()->getInitValue() + problem->getObj()->getNodeCost(node));

    std::vector<Resource*>& resources = problem->getResources();
    for(int id = 0; id < problem->getNumRes(); id++)
        current_label->setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(node));

    for(auto node: tour) {
        Label new_label;
        extendLabel(current_label, new_label, node);
        tourLabels.push_back(new_label);
        current_label = & tourLabels.back();
    }

    return tourLabels;
}

/** Next Candidate Management **/
bool LMacyclic::candidatesAvailable(){
    bool found_f = false;
    bool found_b = false;

    //If a bucket is empty, moves to the first non-empty one
    while(getBucketValueF() <= incumbent) {
        found_f = not bucket_f[pos_f].empty();
        if(found_f)
            break;
        else
            pos_f++;
    }
    while(getBucketValueB() <= incumbent) {
        found_b = not bucket_b[pos_b].empty();
        if(found_b)
            break;
        else
            pos_b++;
    }
    return (found_f or found_b);
}

//Returns the best cadidate available
Label* LMacyclic::getCandidate() {
    Label* candidate = nullptr;

    if(getBucketValueF() < getBucketValueB()) {
        candidate = bucket_f[pos_f].front();
        bucket_f[pos_f].pop_front();
    }
    else {
        candidate = bucket_b[pos_b].front();
        bucket_b[pos_b].pop_front();
    }

    return candidate;
}

/** Label management **/
//Extends a label to another node
void LMacyclic::extendLabel(Label* current_label, Label & new_label, int next_node) {
    new_label = *current_label;
    new_label.updateLabel(next_node, current_label);
    bool direction = current_label->getDirection();

    int i = direction ? current_label->getNode() : next_node;
    int j = direction ? next_node : current_label->getNode();

    //Update Values
    cost_t current_obj, new_obj;
    int current_value, new_value;

    //Update Objective
    current_obj = current_label->getObjective();
    new_obj = obj->extend(current_obj, i, j, direction);
    new_label.setObjective(new_obj);

    //Update res
    current_value = current_label->getSnapshot(RES_CRITICAL);
    new_value = res->extend(current_value, i, j, direction);
    new_label.setSnapshot(RES_CRITICAL, new_value);
}

//Inserts
bool LMacyclic::insert(Label& candidate) {

    int node = candidate.getNode();
    bool direction = candidate.getDirection();

    auto cost_label = bound_labels->getLabel(RES_COST, direction, node);
    auto res_label = bound_labels->getLabel(RES_CRITICAL, direction, node);

    if(candidate.getObjective() > res_label->getObjective())
        return false;

    if(candidate.getSnapshot(RES_CRITICAL) > cost_label->getSnapshot(RES_CRITICAL))
        return false;

    auto cost_label_opp = bound_labels->getLabel(RES_COST, not direction, node);
    auto res_label_opp = bound_labels->getLabel(RES_CRITICAL, not direction, node);

    cost_t value = candidate.getObjective() + cost_label_opp->getObjective();

    if(value > incumbent)
        return false;

    if(res_label_opp->isObjectiveSet())
        if(not res->isFeasible(candidate.getSnapshot(RES_CRITICAL) + res_label_opp->getSnapshot(RES_CRITICAL)))
            return false;

    auto & min_consumption = direction ? min_consumption_f[node] : min_consumption_b[node];
    if(candidate.getSnapshot(RES_CRITICAL) >= min_consumption)
        return false;

    auto& bucket = direction ? bucket_f[value - offset_f] : bucket_b[value - offset_b];
    Label* ins = new Label();
    *ins = candidate;
    bucket.push_back(ins);
    direction ? nlabels_fw++ : nlabels_bw++;

    return true;
}

//Check if a label is dominated
bool LMacyclic::isDominated(Label *candidate) {
    int node = candidate->getNode();
    auto & min_consumption = candidate->getDirection() ? min_consumption_f : min_consumption_b;

    if(candidate->getSnapshot(RES_CRITICAL) < min_consumption[node]) {
        min_consumption[node] = candidate->getSnapshot(RES_CRITICAL);
        return false;
    }

    return true;
}

//Check if extension is feasible
bool LMacyclic::isExtensionFeasible(Label *candidate) {
    bool direction = candidate->getDirection();
    int node = candidate->getNode();
    int value = candidate->getSnapshot(RES_CRITICAL);
    double split = direction ? split_ratio : 1 - split_ratio;
    return res->isFeasible(value, node, split, direction);
}

//Check if incumbent can be updated
bool LMacyclic::earlyUpdate(Label *candidate) {
    int node = candidate->getNode();
    bool direction = candidate->getDirection();
    auto cost_label_opp = bound_labels->getLabel(RES_COST, not direction, node);

    cost_t value = obj->join(candidate->getObjective(), cost_label_opp->getObjective(), node);
    int res_consumption = candidate->getSnapshot(RES_CRITICAL) + cost_label_opp->getSnapshot(RES_CRITICAL);

    if(value >= incumbent)
        return false;
    else if(res->isFeasible(res_consumption, node)) {
        incumbent = value;
        earlyImprov++;
        return true;
    }

    auto res_label_opp = bound_labels->getLabel(RES_CRITICAL, not direction, node);
    if(not res_label_opp->isObjectiveSet())
        return false;

    value = candidate->getObjective() + res_label_opp->getObjective();
    res_consumption = candidate->getSnapshot(RES_CRITICAL) + res_label_opp->getSnapshot(RES_CRITICAL);
    if(value < incumbent and res->isFeasible(res_consumption)){
        incumbent = value;
        earlyImprov++;
        return true;
    }

    return false;
}

/** Join management **/
//Perform join procedures
void LMacyclic::join(Label* current) {
    int node = current->getNode();
    bool direction = current->getDirection();
    auto & opposite_closed = direction ? closed_b[node] : closed_f[node];

    double bounding_opp = direction ? 1 - split_ratio : split_ratio;
    Label* res_label_opp = bound_labels->getLabel(RES_CRITICAL, not direction, node);
    if(res_label_opp->isObjectiveSet()) {
        int res_bound_opp = res_label_opp->getSnapshot(RES_CRITICAL);
        if(not res->isFeasible(res_bound_opp, node, bounding_opp, direction))
            return;
    }

    if(not opposite_closed.empty()) {
        cost_t cost;
        int consumption;
        for (auto* closed: opposite_closed) {
            cost = obj->join(current->getObjective(),closed->getObjective(), node);
            if(cost > incumbent)
                break;

            joinComparisons++;
            consumption = res->join(current->getSnapshot(RES_CRITICAL), closed->getSnapshot(RES_CRITICAL), node);
            if(res->isFeasible(consumption, node)){
                incumbent = cost;
                best_label_pair = std::make_tuple(incumbent, *current, *closed);
                break;
            }
        }
    }
}

/** Data collection management **/
void LMacyclic::initDataCollection(){
    if(not Parameters::isCollecting())
        return;

    collector.init("lm_name", name);
    collector.init("data_structure", lm_type);
    collector.init("executionID", 0);
    collector.init("iterations", 0);

    collector.init("nlabels", 0);
    collector.init("nfw", 0);
    collector.init("nbw", 0);
    collector.init("earlyImprov", 0);
    collector.init("nbuckets", 0);
    collector.init("nclosed", 0);
    collector.init("nclosed_fw", 0);
    collector.init("nclosed_bw", 0);
    collector.init("njoin", "0");

    collector.init("split", split_ratio);
    collector.setHeader();
}

void LMacyclic::collectData(){
    if(not Parameters::isCollecting())
        return;

    collector.collect("lm_name", name);
    collector.collect("executionID", executionID);
    collector.collect("iterations", 1);
    collector.collect("nlabels", nlabels_fw + nlabels_bw);
    collector.collect("nfw", nlabels_fw);
    collector.collect("nbw", nlabels_bw);
    collector.collect("nbuckets", (int) (bucket_f.size() + bucket_b.size()));
    collector.collect("nclosed", nclosed_fw + nclosed_bw);
    collector.collect("nclosed_fw", nclosed_fw);
    collector.collect("nclosed_bw", nclosed_bw);
    collector.collect("njoin", std::to_string(joinComparisons));
    collector.collect("split", split_ratio);
    collector.collect("earlyImprov", earlyImprov);
    collector.saveRecord();
}
