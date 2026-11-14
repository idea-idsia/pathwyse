#include "algorithms/preprocessing/dijkstra.h"

// Algorithm management

/**
* Constructor. Builds a Dijkstra algorithm object.
* @param name - Algorithm name.
* @param problem - Problem (pointer).
*/
Dijkstra::Dijkstra(std::string name, Problem* problem): Algorithm(name, problem) {
    visited = Bitset(problem->getNumNodes());
    distances.resize(problem->getNumNodes(), MAX_COST);
    lower_bound = MIN_COST;
    obj = problem->getObj();
    resources = problem->getResources();

    found_optimal = false;
    round = 0;

    //Init incumbent solution (cost, label 1, label 2)
    solution_data =  std::make_tuple(MAX_COST, nullptr, nullptr);
    setStatus(ALGO_READY);
}

/**
* Initializes Dijkstra algorithm.
* 
* @param direction - Algorithm direction. True if forward, backward otherwise.
* @param res_id - Position of the resource that will be used as costs by the algorithm.
* @param bounding - Resource budget (for critical resource only).
*/
void Dijkstra::initAlgorithm(bool direction, int res_id, double bounding) {
    bound_labels = problem->getBoundLabels();
    this->direction = direction;
    this->res_id = res_id;
    this->bounding = bounding;
    origin = direction ? problem->getOrigin() : problem->getDestination();
    destination = direction ? problem->getDestination() : problem->getOrigin();
    labels = res_id == RES_COST ? bound_labels->getCostLabels(direction) : bound_labels->getConsumptionLabels(res_id, direction);

    int coord_distance_type = res_id == RES_COST ? problem->getObj()->getData()->getCoordDistanceType()
                                                 : problem->getRes(res_id)->getData()->getCoordDistanceType();
    switch(coord_distance_type) {
        case DIST_2D: coord_distance_algo = DIST_ALGO_EUCLIDEAN; break;
        case DIST_GEOGRAPHIC: coord_distance_algo = DIST_ALGO_EQUIRECTANGULAR; break;
        default: coord_distance_algo = DIST_ALGO_NONE; break;
    }
}

/**
* Solves the problem.
*/
void Dijkstra::solve() {
    auto start_t = std::chrono::system_clock::now();
    setStatus(ALGO_OPTIMIZING);

    // Cost of Dijkstra algorithm
    cost_t dijkstra_cost;
    // Temporary values for obj/resource cost/consumption for current label and new labels
    cost_t current_obj, new_obj;
    int current_value, new_value;

    Label new_label;

    // Initialization
    int current = origin;

    // Get origin label and update initial values
    Label* current_label = & labels->at(current);
    current_label->initLabel(origin, nullptr, direction, problem->getNumRes());
    current_label->setObjective(obj->getInitValue() + obj->getNodeCost(origin));
    for(int id = 0; id < problem->getNumRes(); id++)
        current_label->setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(origin));

    // Initialize Dijkstra queues
    distances[current] = getDistance(current_label) + problem->getCoordDistance(current, destination, coord_distance_algo);
    pq.emplace(distances[current], current);

    // While the queue is not empty
    while (not pq.empty()) {
        // Current node to extend
        current = pq.top().second;

        // Do not visit already closed nodes
        if (visited.get(current)) {
            pq.pop();
            continue;
        }
        // Extending current node
        visited.set(current);

        // Get the label of the node and the respective Dijkstra cost
        current_label = &labels->at(current);
        dijkstra_cost = pq.top().first;

        // Check if label is valid (i.e. Not suboptimal or infeasibile)
        if (not isLabelValid(current_label, dijkstra_cost))
            break;

        pq.pop();

        // Try to find feasible solutions with completion labels
        if(round > 0)
            joinCompletion(current_label);

        // Check if critical label is valid (i.e. Not suboptimal or infeasibile) with respect the allowed budget for the direction
        if (not isCriticalLabelValid(current_label))
            continue;

        // Get reachable nodes
        auto &neighbors = problem->getNeighbors(current, direction);
        for (auto &node: neighbors)
            //Extend towards active and unvisited nodes
            if (problem->isActiveNode(node) and not visited.get(node)) {
                // Make a new label: copy current label and update it
                new_label = *current_label;
                new_label.updateLabel(node, current_label);

                int i = direction ? current_label->getNode() : node;
                int j = direction ? node : current_label->getNode();

                //Update Objective for new label
                current_obj = current_label->getObjective();
                new_obj = obj->extend(current_obj, i, j, direction);
                new_label.setObjective(new_obj);

                //If Dijkstra is minimizing objective cost, check if the new label is improving the minimum cost found at the node
                if(res_id == RES_COST) {
                    dijkstra_cost = getDistance(&new_label) + problem->getCoordDistance(node, destination, coord_distance_algo);
                    if(distances[node] <= dijkstra_cost)
                        continue;
                }

                //Update resources for new label
                for(int id = 0; id < problem->getNumRes(); id++){
                    current_value = current_label->getSnapshot(id);
                    new_value = resources[id]->extend(current_value, i, j, direction);
                    new_label.setSnapshot(id, new_value);
                }

                //If Dijkstra is minimizing a resource consumption, check if the new label is improving the minimum consumption found at the node
                if(res_id != RES_COST) {
                    dijkstra_cost = getDistance(&new_label) + problem->getCoordDistance(node, destination, coord_distance_algo);
                    if(distances[node] <= dijkstra_cost)
                        continue;
                }

                //If feasible with respect to incumbent/upper bounds, update data structures
                if (isLabelValid(current_label, dijkstra_cost)) {
                    distances[node] = dijkstra_cost;
                    labels->at(node) = new_label;
                    pq.emplace(distances[node], node);
                }
            }
    }

    //Check Feasibility guarantees
    checkFeasibility();

    //Check if a feasible solution was found
    checkOptimality();

    round++;

    //Print status
    if(Parameters::getVerbosity() >= 3) {
        auto end_t = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end_t-start_t;
        std::cout << "Time: " << elapsed_seconds.count() << std::endl;

        if(res_id == RES_COST){
            std::cout<<"Min cost: " << distances[destination]<< std::endl;
            std::cout<<"Distance Type: " << obj->getData()->getCoordDistanceType() << std::endl;
        }
        else{
            std::cout<<"Min consumption: " << distances[destination]<<std::endl;
            std::cout<<"Distance Type: " << resources[res_id]->getData()->getCoordDistanceType() << std::endl;
        }

    }

    setStatus(ALGO_DONE);
}


/**
* Checks if a label is valid.
* For the objective function: the cost must be lesser or equal to the incumbent.
* For resources: their consumption must be lesser or equal to their budget.
* 
* @param l - Label (pointer).
* @param cost - The cost of the label.
* @return True if the label is valid, False otherwise.
*/
bool Dijkstra::isLabelValid(Label* l, cost_t cost) {
    if(res_id == RES_COST)
        return cost <= incumbent;
    else
        return resources[res_id]->isFeasible((int)cost, l->getNode(), 1, l->getDirection());
}

/**
* Extends a label to the next node
* 
* @param current_label - The current label to extend from (pointer).
* @param new_label - The label to extend to (reference).
* @param next_node - The node to extend to.
*/
void Dijkstra::extendLabel(Label* current_label, Label & new_label, int next_node) {
    new_label = *current_label;
    new_label.updateLabel(next_node, current_label);

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
    for(int id = 0; id < problem->getNumRes(); id++){
        current_value = current_label->getSnapshot(id);
        new_value = resources[id]->extend(current_value, i, j, direction);
        new_label.setSnapshot(id, new_value);
    }
}

/**
* Checks if the consumption of the critical resource a label is allowed by the current budget.
* 
* @param l - Label (pointer).
* @return True if consumption is feasible, False otherwise.
*/
bool Dijkstra::isCriticalLabelValid(Label* l){
    if(res_id == RES_CRITICAL)
        return resources[RES_CRITICAL]->isFeasible(l->getSnapshot(RES_CRITICAL), l->getNode(), bounding, l->getDirection());

    return true;
}

/**
* Resets the algorithm to the initial state.
* 
* @param reset_level
*/
void Dijkstra::resetAlgorithm(int reset_level) {
    visited.reset();
    std::fill(distances.begin(), distances.end(), MAX_COST);
    setStatus(ALGO_READY);
}


/**
* Attempts to join current label with its cost or critical resource completion labels.
* 
* @param current - Label (pointer).
*/
void Dijkstra::joinCompletion(Label* current){
    int node = current->getNode();
    bool stop_early = false;
    bool direction_opp = not current->getDirection();

    //Try joining current label with a cost label (in the other direction)
    Label* completion_label = bound_labels->getLabel(RES_COST, direction_opp, node);
    if(completion_label->getObjective() < incumbent)
        stop_early = joinLabels(current, completion_label);

    //Try joining current label with a critical resource label (in the other direction)
    //Join only if: previous join had a better incumbent objective but was infeasible
    if(not stop_early) {
        completion_label = bound_labels->getLabel(RES_CRITICAL, direction_opp, node);
        if(completion_label->getObjective() < incumbent)
            stop_early = joinLabels(current, completion_label);
    }
}


/**
* Attempts to join two labels.
* The join succedes if the resulting path is feasible and improves the incumbent.
* 
* @param l1 - First label (pointer).
* @param l2 - Second label (pointer).
* @return True if the labels were joined, False otherwise.
*/
bool Dijkstra::joinLabels(Label* l1, Label* l2){
    int node = l1->getNode();
    cost_t cost;
    int consumption;

    //Find the objective of the join
    if(direction)
        cost = obj->join(l1->getObjective(), l2->getObjective(), node);
    else
        cost = obj->join(l2->getObjective(), l1->getObjective(), node);

    //Return true if suboptimal
    if(cost >= incumbent)
        return true;


    //Return False if a resource is infeasible
    for(int id = 0; id < problem->getNumRes(); id++){
        if(direction)
            consumption = resources[id]->join(l1->getSnapshot(id), l2->getSnapshot(id), node);
        else
            consumption = resources[id]->join(l2->getSnapshot(id), l1->getSnapshot(id), node);

        if(not resources[id]->isFeasible(consumption, node))
            return false;
    }

    //The join improves the incumbent solution and is feasible
    incumbent = cost;
    std::get<0>(solution_data) = incumbent;
    std::get<1>(solution_data) = l1;
    std::get<2>(solution_data) = l2;

    return true;
}


/**
* Checks if there is a feasible solution and possibly update status.
*/
void Dijkstra::checkFeasibility(){
    if(round == 0 and Parameters::isPreprocessingCritical())
        return;

    //If status has already been set, return
    if(problem->getStatus() != PROBLEM_INDETERMINATE)
        return;

    //If no path could reach the destination, the problem is infeasible
    if(distances[destination] == MAX_COST) {
        problem->setStatus(PROBLEM_INFEASIBLE);
        return;
    }

    //If a feasible solution was found, the problem is feasible
    if(incumbent != MAX_COST) {
        problem->setStatus(PROBLEM_FEASIBLE);
        return;
    }

    //If a single resource is present, and Dijkstra found a path while minimizing consumptions,
    //Then the problem is feasible (and a feasible solution has been found at the destination node)
    if(res_id == RES_CRITICAL and problem->getNumRes() == 1){
        problem->setStatus(PROBLEM_FEASIBLE);
        Label* l = bound_labels->getLabel(res_id, direction, destination);
        if(updateIncumbent(l->getObjective())){
            std::get<0>(solution_data) = incumbent;
            std::get<1>(solution_data) = l;
        }
    }
}


/**
* Checks for optimality.
*/
void Dijkstra::checkOptimality(){
    if(res_id == RES_COST and distances[destination] != MAX_COST)
        lower_bound = distances[destination];

    if(lower_bound == incumbent)
        found_optimal = true;
}

// Solution management

/**
* Attempts to build a path.
* Only used when there is the guarantee that the optimal solution has been found by a Dijkstra run.
*/
void Dijkstra::managePaths() {

    auto [objective, l1, l2] = solution_data;
    Label* fw = nullptr;
    Label* bw = nullptr;

    if(l1 and l2 and l1->getNode() == l2->getNode())
        l1 = l1->getPredecessor(); //Removes duplicate node when performing node join

    if (l1 or l2) {
        fw = (l1 and l1->getDirection()) or (l2 and not l2->getDirection()) ? l1 : l2;
        bw = (l1 and l1->getDirection()) or (l2 and not l2->getDirection()) ? l2 : l1;
    }

    buildPath(objective, fw, bw);

    auto bestPath = getBestSolution();
    if(not bestPath)
        return;

    bestPath->setStatus(PATH_OPTIMAL);
    std::list<Label> tourFW = buildTour(bestPath->getTour(), true);
    bestPath->setConsumption(tourFW.back().getSnapshot());
}

/**
* Builds a tour and returns the list of labels.
* 
* @param tour - List of visited nodes.
* @param direction - Direction of the tour. True if forward, false otherwise.
*/
std::list<Label> Dijkstra::buildTour(std::list<int> tour, bool direction) {
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