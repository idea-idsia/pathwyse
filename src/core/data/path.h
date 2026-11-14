#ifndef PATH_H
#define PATH_H

#include <list>
#include "algorithms/labels/label.h"

class Path {

public:

    /** Path management **/
    //Constructors and destructors
    Path() {initPath();}
    Path(const Path &obj);
    ~Path() = default;

    void initPath();
    void reset() {initPath();}

    /** Tour management **/
    std::list<int> getTour(){return tour;}
    std::string getTourAsString();
    void setTour(std::list<int> tour);
    int getTourLength(){return tour.size();}

    //Elementarity management
    bool isElementary() {return elementary;}
    void setElementary(bool elementary) {this->elementary = elementary;}
    bool checkElementarity();

    /** Status and solution management **/
    int getStatus(){return solution_status;}
    void setStatus(int status){solution_status = status;}

    //Objective and resource information
    cost_t getObjective(){return objective;}
    void setObjective(cost_t objective){this->objective = objective;}
    cost_t getArcCost() {return arc_cost;}
    void setArcCost(cost_t cost) {this->arc_cost = cost;}
    cost_t getNodeCost() {return node_cost;}
    void setNodeCost(cost_t cost) {this->node_cost = cost;}
    int getConsumption(int id){return consumption[id];}
    std::vector<int> getConsumptions(){return consumption;}
    void setConsumption(std::vector<int> consumption){this->consumption = consumption;}

    //Optimality guarantee
    bool isOptimal() {return solution_status == PATH_OPTIMAL;}
    bool isFeasible(){return solution_status == PATH_FEASIBLE;}

    /** Print path information **/
    void printPath(double init_cost = 0.0, double cost_scale = 1.0);
    void printTour();
    void printStatus();

    /** Labels management **/
    void setLabels(std::list<Label> labels){this->labels = labels;}
    std::list<Label> & getLabels(){return labels;}


private:
    int solution_status;                 //Solution status

    std::list<int> tour;                 //Tour elements
    bool elementary;

    cost_t objective;                   //Objective value
    cost_t arc_cost, node_cost;
    std::vector<int> consumption;       //Resources consumption

    std::list<Label> labels;            //Path labels
};

/** Path Comparison **/
//Objective and Status based
struct less_than_objective_status
{
    bool operator() (Path& p1, Path& p2){
        return (
            p1.getObjective() < p2.getObjective() or 
            (p1.getObjective() == p2.getObjective() and p1.getStatus() < p2.getStatus())
        );
    }
};

#endif