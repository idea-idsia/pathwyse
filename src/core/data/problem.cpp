#include "problem.h"
#include <iostream>
#include <iterator>
#include <sstream>
#include <cmath>
#include <utils/logger.h>

/** Problem Management **/
//Constructors and destructors
Problem::Problem() {
    name = "undefined";
    n_nodes = 0;
    n_pairs = 0;
    origin = 0;
    destination = 0;
    n_res = 0;
    n_arcs = 0;
    cost_scale_factor = 1.0;
    duplicate_depot = false;
    directed_set = false;
    cyclic_set = false;
    symmetric_set = false;
    cycles = false;
    init_done = false;
    build_done = false;
    objective = nullptr;
    bound_labels = nullptr;
    setStatus(PROBLEM_INDETERMINATE);
    //Initialize Data Collection
    initDataCollection();
}

Problem::~Problem() {
    delete objective;
    delete bound_labels;
    for(auto& r: resources)
        delete r;
}

//Initialize Problem
void Problem::init(){
    if (n_nodes <= 0) {
        Logger::error("Network has no nodes");
        return;
    }

    // Apply defaults for flags not explicitly set, with warnings
    if(not directed_set) {
        directed = false;
        Logger::warn("Directed not set, assuming undirected edges");
    }

    if(not symmetric_set) {
        symmetric = false;
        Logger::warn("Symmetric not set, assuming asymmetric network");
    }

    if(not cyclic_set) {
        cycles = false;
        Logger::warn("Cyclic not set, assuming acyclic network");
    }

    //Use Data Compression for sparse data
    compress_data = n_nodes > Parameters::getCompressionThreshold();

    //Duplicate origin node
    if(origin == destination){
        duplicate_depot = true;
        destination = n_nodes++;
        Logger::warn("Duplicating depot node. Destination is now: " + std::to_string(destination));
    }


    //Initialize Network
    network.initGraph(compress_data, n_nodes);

    //Initialize Objective data structures
    initObjective();

    init_done = true;

}

void Problem::printStatus(){
    std::string status;

    switch(problem_status){
        case PROBLEM_INDETERMINATE:
            status = "Indeterminate";
            break;
        case PROBLEM_INFEASIBLE:
            status = "Infeasible";
            break;
        case PROBLEM_FEASIBLE:
            status = "Feasible";
            break;
        default:
            break;
    }

    Logger::log("Problem Status", status);
}

/** Objective and Resource management **/
//Initialize Objective data structures
void Problem::initObjective(Resource *objective) {
    //If a custom objective is not defined, use default objective
    if(not objective)
        objective = new DefaultCost();
    this->objective = objective;

    //Initialize data structures
    this->objective->initData(compress_data, n_nodes);
}

//Initialize a resource Data Object
int Problem::addResource(std::string type) {
    Resource* res;
    int index;

    if (type == "CAP") res = new Capacity();
    else if (type == "TIME")  res = new Time();
    else if (type == "TW") res = new TimeWindow();
    else if (type == "NODELIM") res = new NodeLim();
    else {
        Logger::warn("Resource " + type + "has no match and cannot be instantiated.");
        return -1;
    }

    resources.push_back(res);
    res->initData(compress_data, n_nodes);
    index = resources.size()-1;
    return index;
}

void Problem::createResources(std::vector<std::string> & resources_type){
    for(auto rt: resources_type) addResource(rt);
}

int Problem::getCoordDistanceType(std::string distance) {
    if(distance == "2D")
        return DIST_2D;
    if (distance == "GEO")
        return DIST_GEOGRAPHIC;

    return DIST_NONE;
}

int Problem::getMinConsumptionAtExtension(int res_id)
{
    int min_consumption = INFPLUS;
    for(int i = 0; i < n_nodes; i++) {
        for(auto j: network.getNeighbors(i, true)) {
            if (j == origin or j == destination)
                continue;
            int consumption = resources[res_id]->extend(0,i,j,true);
            if (consumption < min_consumption)
                min_consumption = consumption;
        }
    }
    return min_consumption;
}

/** Read from instance file **/
//Read and initialize data (extended reader)
void Problem::readProblem(std::string file_name) {
    std::ifstream f;
    std::string line, key, label, separator, res_name, res_type, coord_distance_id;
    std::vector<std::string> tokens;
    int res_id, i, j;
    int lb, ub, consumption;
    double cost;
    int coord_distance_type = DIST_NONE;

    Logger::log("Reading problem data: " + file_name);

    //Read from file
    f.open(file_name.c_str());

    while(readNextLine(f, line, tokens, key)) {
        //Read initialization data
        if (key == "NAME") setName(tokens[2]);
        else if (key == "SIZE"){
            setNumNodes(std::stoi(tokens[2]));
            //If present, override origin and destination with console input
            if(Parameters::getOrigin() != -1)
                setOrigin(Parameters::getOrigin());

            if(Parameters::getDestination() != -1)
                setDestination(Parameters::getDestination());
        }
        else if (key == "DIRECTED") setDirected(std::stoi(tokens[2]));
        else if (key == "CYCLIC") setCyclic(std::stoi(tokens[2]));
        else if (key == "SYMMETRIC") setSymmetric(std::stoi(tokens[2]));
        else if (key == "ORIGIN") {
            if(Parameters::getOrigin() == -1)
                setOrigin(std::stoi(tokens[2]));
        }
        else if (key == "DESTINATION") {
            if(Parameters::getDestination() == -1)
                setDestination(std::stoi(tokens[2]));
        }
        else if (key == "RES_TYPE") {
            //Initialize problem
            init();
            readNextLine(f, line, tokens, key);
            while(key != "END") {
                //Initialize resource objects
                addResource(tokens[1]);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "RES_BOUND") {
            //Read lower bound and upperbound for each resource
            readNextLine(f, line, tokens, key);

            while (key != "END") {
                res_id = std::stoi(tokens[0]);
                lb = std::stoi(tokens[1]);
                ub = std::stoi(tokens[2]);
                if(Parameters::getCriticalUB(res_id) != -1)
                    ub = Parameters::getCriticalUB(res_id);
                setResBounds(res_id, lb, ub);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "RES_NODE_BOUND") {
            //For each node i, add lower and upper bound for a given resource
            readNextLine(f, line, tokens, key);

            while (key != "END") {
                res_id = std::stoi(tokens[0]);
                i = std::stoi(tokens[1]);
                lb = std::stoi(tokens[2]);
                ub = std::stoi(tokens[3]);
                setResNodeBound(res_id, i, lb, ub);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "EDGE_COST") {
            //For each arc i-j, read a cost
            readNextLine(f, line, tokens, key);

            while (key != "END") {
                i = std::stoi(tokens[0]);
                j = std::stoi(tokens[1]);
                cost = std::stod(tokens[2]);
                addArc(i, j);
                setArcCost(i, j, cost);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "NODE_COST") {
            //For each node i, read a cost
            readNextLine(f, line, tokens, key);
            while (key != "END") {
                i = std::stoi(tokens[0]);
                cost = std::stod(tokens[1]);
                setNodeCost(i, cost);
                readNextLine(f, line, tokens, key);
            }
        }

        else if(key == "EDGE_CONSUMPTION") {
            //For each arc i-j, read a resource consumption
            readNextLine(f, line, tokens, key);

            while (key != "END") {
                res_id = std::stoi(tokens[0]);
                i = std::stoi(tokens[1]);
                j = std::stoi(tokens[2]);
                consumption = std::stoi(tokens[3]);
                setResArcConsumption(res_id, i, j, consumption);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "NODE_CONSUMPTION") {
            //For each node i, read a resource consumption
            readNextLine(f, line, tokens, key);

            while (key != "END") {
                res_id = std::stoi(tokens[0]);
                i = std::stoi(tokens[1]);
                consumption = std::stoi(tokens[2]);
                setResNodeConsumption(res_id, i, consumption);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "COORD_DISTANCE_TYPE") {
            //Read distance type (2d, Geographic, none)
            readNextLine(f, line, tokens, key);

            while (key != "END") {
                coord_distance_id = tokens[0];
                coord_distance_type = getCoordDistanceType(tokens[1]);
                setCoordinatesType(coord_distance_id, coord_distance_type);
                readNextLine(f, line, tokens, key);
            }
        }

        else if (key == "COORD") {
            //For each node i, read coordinates (x,y)
            readNextLine(f, line, tokens, key);
            while (key != "END") {
                i = std::stoi(tokens[0]);
                int xval = std::stoi(tokens[1]);
                int yval = std::stoi(tokens[2]);
                setCoordinates(i, xval, yval);
                readNextLine(f, line, tokens, key);
            }
        }
    }

    f.close();

    Logger::log("Reading problem data complete");
    Logger::divider();

    //Initialize resources
    build();
}

void Problem::scaleCosts() {
    double max_abs = objective->getMaxAbsValue();
    int requested_decimal_digits = std::max(0, Parameters::getDecimalDigits());
    if (requested_decimal_digits != Parameters::getDecimalDigits())
        Logger::warn("problem/decimal_digits must be non-negative. Using 0.");

    int integer_digits = 1;
    if (max_abs >= 1.0) {
        int temp = (int) max_abs;
        while (temp >= 10) {
            temp /= 10;
            integer_digits++;
        }
    }
    int available = std::max(0, MAX_COST_DIGITS - integer_digits);

    if (integer_digits > MAX_COST_DIGITS)
        Logger::error("Max cost value requires " + std::to_string(integer_digits) +
                      " digits, exceeding MAX_COST_DIGITS (" + std::to_string(MAX_COST_DIGITS) + ").");

    int scale_digits = std::min(requested_decimal_digits, available);
    if (requested_decimal_digits > available)
        Logger::warn("Requested " + std::to_string(requested_decimal_digits) + " decimal digits but only " +
                     std::to_string(available) + " available (MAX_COST_DIGITS=" +
                     std::to_string(MAX_COST_DIGITS) + ", integer digits=" +
                     std::to_string(integer_digits) + "). Scaling with " +
                     std::to_string(scale_digits) + ".");

    cost_scale_factor = std::pow(10.0, scale_digits);
    objective->applyScale(cost_scale_factor);
}

void Problem::build(){
    for(auto r: resources)
        r->init(origin, destination);
    n_res = resources.size();
    build_done = true;
    collectData();
    printProblem();
}

//Support methods for read procedure
void Problem::getTokens(std::string & line, std::vector<std::string> & tokens){
    tokens.clear();
    std::istringstream iss(line);
    tokens = {std::istream_iterator<std::string>{iss},std::istream_iterator<std::string>{}};
}

bool Problem::readNextLine(std::ifstream & f, std::string & line, std::vector<std::string> & tokens, std::string & key){
    if(getline(f, line)) {
        getTokens(line, tokens);
        key = tokens[0];
        return true;
    }
    return false;
}


/** Construction interface **/
int Problem::setNumNodes(int n) {
    if(n <= 1) {
        Logger::error("Number of nodes must be greater than 1");
        return RETURN_ERROR;
    }
    n_nodes = n;
    Logger::debug("Set number of nodes: ", n);
    return RETURN_OK;
}

int Problem::setPair(int pickup, int delivery) {
    if(not nodeExists(pickup) or not nodeExists(delivery))
        return RETURN_ERROR;
    if(pickup == delivery) {
        Logger::error("Pickup and delivery of a pair must differ");
        return RETURN_ERROR;
    }
    if(pickup == origin or pickup == destination or delivery == origin or delivery == destination) {
        Logger::error("Origin and destination cannot be part of a pair");
        return RETURN_ERROR;
    }
    if((int)is_pickup.size() < n_nodes) {
        is_pickup.resize(n_nodes, 0);
        pickup_of.resize(n_nodes, -1);
    }
    if(is_pickup[pickup] or pickup_of[pickup] >= 0 or is_pickup[delivery] or pickup_of[delivery] >= 0) {
        Logger::error("A node can belong to one pair only");
        return RETURN_ERROR;
    }
    is_pickup[pickup] = 1;
    pickup_of[delivery] = pickup;
    n_pairs++;
    return RETURN_OK;
}

int Problem::setOrigin(int origin) {
    if (not nodeExists(origin)) return RETURN_ERROR;
    this->origin = origin;
    Logger::debug("Set origin: ", origin);
    return RETURN_OK;
}

int Problem::setDestination(int destination) {
    if (not nodeExists(destination)) return RETURN_ERROR;
    this->destination = destination;
    Logger::debug("Set destination: ", destination);
    return RETURN_OK;
}

int Problem::setDirected(bool directed) {
    this->directed = directed;
    directed_set = true;
    Logger::debug("Set directed: ", directed);
    return RETURN_OK;
}

int Problem::setSymmetric(bool symmetric) {
    this->symmetric = symmetric;
    symmetric_set = true;
    Logger::debug("Set symmetric: ", symmetric);
    return RETURN_OK;
}

int Problem::setCyclic(bool cyclic) {
    this->cycles = cyclic;
    cyclic_set = true;
    Logger::debug("Set cyclic: ", cyclic);
    return RETURN_OK;
}

int Problem::setResBounds(int res_id, int lb, int ub) {
    if (not resExists(res_id)) return RETURN_ERROR;

    if(lb > ub) {
        Logger::error("Resource lower bound " + std::to_string(lb) + " exceeds upper bound " + std::to_string(ub) + " for res " + std::to_string(res_id));
        return RETURN_ERROR;
    }
    resources[res_id]->setBounds(lb, ub);
    Logger::debug("Set bounds [", lb, ", ", ub, "] on resource ", resources[res_id]->getName());
    return RETURN_OK;
}

int Problem::setResNodeBound(int res_id, int node, int lb, int ub) {
    if (not resExists(res_id)) return RETURN_ERROR;
    if (not nodeExists(node)) return RETURN_ERROR;

    if(lb > ub) {
        Logger::error("Resource lower bound " + std::to_string(lb) + " exceeds upper bound " + std::to_string(ub) + " for res " + std::to_string(res_id) + "on node " + std::to_string(node));
        return RETURN_ERROR;
    }
    resources[res_id]->setNodeBound(n_nodes, node, lb, ub);
    if (duplicate_depot and node == origin){
        if (lb == 0 and ub == 0){
            Logger::warn("Destination TW upper bound is 0. Changing it to MAX_INT.");
            ub = UNKNOWN;
        }
        resources[res_id]->setNodeBound(n_nodes, destination, lb, ub);
    }
    Logger::debug("Set node bound [", lb, ", ", ub, "] at node ", node, " on resource ", resources[res_id]->getName());
    return RETURN_OK;
}


void Problem::setCoordinates(int i, int x, int y) {
    network.setxy(i, x, y);
    if(duplicate_depot and i == origin)
        network.setxy(destination, x, y);
}

void Problem::setCoordinatesType(std::string coord_distance_id, int coord_distance_type){
    if(coord_distance_id != "OBJ"){
        int res_id = std::stoi(coord_distance_id);
        resources[res_id]->getData()->setCoordDistanceType(coord_distance_type);
    }
    else
        objective->getData()->setCoordDistanceType(coord_distance_type);
}

int Problem::setBounds(double lb, double ub) {
    if(lb > ub) {
        Logger::error("Objective lower bound " + std::to_string(lb) + " exceeds upper bound " + std::to_string(ub) + " for objective ");
        return RETURN_ERROR;
    }
    objective->setBoundsDouble(lb, ub);
    Logger::debug("Set objective bounds [", lb, ", ", ub, "]");
    return RETURN_OK;
}

bool Problem::nodeExists(int node) const {
    if (node < 0 or node >= n_nodes) {
        Logger::error("Node " + std::to_string(node) + " does not exist");
        return false;
    }
    return true;
}

bool Problem::resExists(int res_id) const{
    if(res_id < 0 or res_id >= resources.size()) {
        Logger::error("Resource not found: " + std::to_string(res_id));
        return false;
    }
    return true;
}

bool Problem::objExists() const {
    if(not objective) {
        Logger::error("Objective not initialized.");
        return false;
    }
    return true;
}

int Problem::addArc(int i, int j){
    if (not nodeExists(i)) return RETURN_ERROR;
    if (not nodeExists(j)) return RETURN_ERROR;

    network.setArc(i, j);
    n_arcs++;
    if (not directed){
        network.setArc(j, i);
        n_arcs++;
    }

    if (not build_done and duplicate_depot and (i == origin or j == origin)) {
        i == origin ? i = destination : j = destination;
        network.setArc(i, j);
        n_arcs++;

        if (not directed) {
            network.setArc(j, i);
            n_arcs++;
        }
    }
    Logger::debug("Added arc (", i, ", ", j);
    return RETURN_OK;
}

int Problem::setArcCost(int i, int j, double cost) {
    if (not nodeExists(i)) return RETURN_ERROR;
    if (not nodeExists(j)) return RETURN_ERROR;

    objective->setArcCostDouble(i, j, cost);

    //Create backward arc, if not directed
    if(not directed)
        objective->setArcCostDouble(j, i, cost);

    //If depot is used, duplicate arcs to destination
    if (not build_done and duplicate_depot and (i == origin or j == origin)) {
        i == origin ? i = destination : j = destination;
        objective->setArcCostDouble(i, j, cost);

        if (not directed)
            objective->setArcCostDouble(j, i, cost);
    }

    Logger::debug("Set arc (", i, ", ", j, ") with cost ", cost);
    return RETURN_OK;
}


int Problem::setNodeCost(int i, double cost) {
    if (not nodeExists(i)) return RETURN_ERROR;

    //If depot is used, split cost between origin and duplicated destination
    if (not build_done and duplicate_depot and i == origin){
        double depot_cost = cost / 2.0;
        objective->setNodeCostDouble(destination, depot_cost);
        cost -= depot_cost;
    }

    objective->setNodeCostDouble(i, cost);

    Logger::debug("Set node ", i, " cost ", cost);
    return RETURN_OK;
}

//Passing a square matrix (needs testing)
void Problem::setArcMatrixCost(std::vector<std::vector<double>> costs) {
    for (int r = 0; r < (int)costs.size(); ++r)
        for (int c = 0; c < (int)costs[r].size(); ++c)
            objective->setArcCostDouble(r, c, costs[r][c]);

    //If depot is used, duplicate origin row and column to destination
    if (not build_done and duplicate_depot)
        for (int i = 0; i < (int)costs.size(); ++i) {
            objective->setArcCostDouble(destination, i, costs[origin][i]);  // row copy
            objective->setArcCostDouble(i, destination, costs[i][origin]);  // column copy
        }
}

int Problem::setNodeCosts(std::vector<double> costs) {

    if (not build_done and duplicate_depot)
        costs.emplace_back(costs[origin]);

    if ((int)costs.size() < n_nodes){
        Logger::error("Cost vector has not enough nodes");
        return RETURN_ERROR;
    }
    for (int i = 0; i < n_nodes; i++)
        objective->setNodeCostDouble(i, costs[i]);

    Logger::debug("Cost set for every node");
    return RETURN_OK;
}

int Problem::setResArcConsumption(int res_id, int i, int j, int cost) {
    if (not resExists(res_id)) return RETURN_ERROR;
    if (not nodeExists(i)) return RETURN_ERROR;
    if (not nodeExists(j)) return RETURN_ERROR;

    resources[res_id]->setArcCost(i, j, cost);

    //Add backward arc consumption, if not directed
    if(not directed)
        resources[res_id]->setArcCost(j, i, cost);

    //If depot is used, duplicate arc consumption to destination
    if (not build_done and duplicate_depot and (i == origin or j == origin)) {
        i == origin ? i = destination : j = destination;
        resources[res_id]->setArcCost(i, j, cost);

        if (not directed)
            resources[res_id]->setArcCost(j, i, cost);
    }

    Logger::debug("Set arc (", i, ", ", j, ") consumption ", cost, " on resource ", resources[res_id]->getName());
    return RETURN_OK;
}

int Problem::setResNodeConsumption(int res_id, int i, int cost){
    if (not resExists(res_id)) return RETURN_ERROR;
    if (not nodeExists(i)) return RETURN_ERROR;

    if (not build_done and duplicate_depot and i == origin){
        int depot_cost = cost/2;
        cost -= depot_cost;
        resources[res_id]->setNodeCost(destination, depot_cost);
    }
    resources[res_id]->setNodeCost(i, cost);

    Logger::debug("Set node ", i, " consumption ", cost, " on resource ", resources[res_id]->getName());
    return RETURN_OK;
}


bool Problem::validate() {
    bool valid = true;

    if (not init_done){
        Logger::error("Init not done");
        valid = false;
    }

    if (not build_done){
        Logger::error("Build not done");
        valid = false;
    }

    if(n_arcs == 0) {
        Logger::error("No arcs defined");
        valid = false;
    }

    return valid;
}

/** Output management **/
//Prints problem Data
void Problem::printProblem() {
    if (Parameters::getVerbosity() < VERB_STD) return;
    using namespace Logger;

    log("[ Problem ]", VERB_STD, BOLD);
    log("Name", name);
    log("Nodes", std::to_string(getNumNodes()));
    log("Resources", std::to_string(n_res));

    log("Origin", std::to_string(origin));
    log("Destination", std::to_string(destination));

    // Resource bounds
    std::string ub_values;
    for (auto &r : resources)
        ub_values += std::to_string(r->getUB()) + " ";
    log("Resource Upper Bounds", ub_values);

    divider();
}

/** Data collection **/
void Problem::initDataCollection(){
    collector = DataCollector("Problem");

    if(not Parameters::isCollecting())
        return;

    collector.init("problem_name", "");
    collector.init("nodes", 0);
    collector.init("arcs", 0);
    collector.init("arcs_per_node", 0.0);
    collector.init("symmetric", -1);
    collector.init("directed", -1);
    collector.init("cyclic", -1);
    collector.init("nres", 0);
    collector.init("nres_complex", 0);
    collector.init("origin", -1);
    collector.init("destination", -1);
    collector.init("obj_lb", 0);
    collector.init("obj_ub", 0);
}

void Problem::collectData(){
    if(not Parameters::isCollecting())
        return;
    collector.collect("problem_name", name);
    collector.collect("nodes", n_nodes);
    collector.collect("arcs", n_arcs);
    collector.collect("arcs_per_node", (double)n_arcs/n_nodes);
    collector.collect("directed", directed);
    collector.collect("cyclic", cycles);
    collector.collect("nres", n_res);
    collector.collect("origin", origin);
    collector.collect("destination", destination);
    collector.collect("obj_lb", getObj()->getLB());
    collector.collect("obj_ub", getObj()->getUB());
    for(int i = 0; i < resources.size(); i++) {
        auto res = getRes(i);
        std::string tag = "r" + std::to_string(i) + "_";
        collector.init(tag+"lb", res->getLB());
        collector.init(tag+"ub", res->getUB());
    }

    collector.setHeader();
    collector.saveRecord();
    collector.writeData();
}
