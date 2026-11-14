#include "path.h"
#include "utils/logger.h"

/** Path management **/

Path::Path(const Path &obj) {
    solution_status = obj.solution_status;
    tour = obj.tour;
    objective = obj.objective;
    arc_cost = obj.arc_cost;
    node_cost = obj.node_cost;
    consumption = obj.consumption;
    labels = obj.labels;
    elementary = obj.elementary;
}

void Path::initPath() {
    solution_status = PATH_UNKNOWN;
    tour.clear();

    elementary = false;
    objective = MAX_COST;
    arc_cost = MAX_COST;
    node_cost = MAX_COST;
    consumption.clear();
    labels.clear();
}

/** Tour management **/

std::string Path::getTourAsString() {
    std::string flat_tour;
    for(auto t: tour)
        flat_tour += std::to_string(t) + " ";
    return flat_tour;
}

void Path::setTour(std::list<int> tour) {
    this->tour = std::move(tour);
    elementary = checkElementarity();
}

//Check if a tour is elementary
bool Path::checkElementarity(){
    auto t = tour;
    t.sort();

    int prev_node = -1;
    for(auto current_node: t) {
        if(prev_node == current_node)
            return false;
        else
            prev_node = current_node;
    }
    return true;
}

/** Output management **/

void Path::printPath(double init_cost, double cost_scale) {
    Logger::log("[ Solution ]", VERB_STD, BOLD);

    printStatus();

    Logger::log("Obj", std::to_string(init_cost + (objective / cost_scale)));
    Logger::log("Tour Length", std::to_string(tour.size()));

    //Tour
    std::string tour_str;
    for (auto &t : tour) tour_str += std::to_string(t) + " ";
    Logger::log("Tour", tour_str);

    //Resources
    std::string cons_str;
    for (auto &c : consumption) cons_str += std::to_string(c) + " ";
    Logger::log("Consumption", cons_str);

    //Labels
    if (Parameters::getVerbosity() == VERB_DATA) {
        for (auto &l : labels)
            l.printLabel();
    }

    Logger::divider();
}

void Path::printTour(){
    std::string tour_str;
    for (auto &t : tour) tour_str += std::to_string(t) + " ";
    Logger::log(tour_str);
}

void Path::printStatus() {
    std::string status;
    const char* col = COL_RESET;

    switch(solution_status){
        case PATH_UNKNOWN: status = "Unknown"; col = RED; break;
        case PATH_OPTIMAL: status = "Optimal"; col = GREEN; break;
        case PATH_FEASIBLE: status = "Feasible"; col = YELLOW; break;
        case PATH_INFEASIBLE: status = "Infeasible"; col = RED; break;
        case PATH_SUPEROPTIMAL: status = "Super optimal"; col = YELLOW; break;
        case PATH_NG: status = "NG-route"; col = YELLOW; break;
        default: break;
    }

    Logger::log("Solution Status", status, VERB_STD, col);
}