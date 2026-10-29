#ifndef PROBLEM_H
#define PROBLEM_H

#include "resources/defaultcost.h"
#include "resources/resource.h"
#include "resources/capacity.h"
#include "resources/time.h"
#include "resources/node_limit.h"
#include "resources/time_windows.h"
#include "resource_data.h"
#include "utils/param.h"
#include "utils/data_collector.h"
#include "graph.h"
#include "bound_data.h"
#include "utils/bitset.h"

class Problem {

public:

    /** Problem Management **/
    //Constructors and destructors
    Problem();
    ~Problem();

    //Name
    std::string getName() {return name;}

    //Status
    int getStatus() {return problem_status;}
    void setStatus(int status) {problem_status = status;}
    void printStatus();

    //Number of nodes
    int getNumNodes(){return n_nodes;}

    //Origin and Destination
    int getOrigin() {return origin;}
    int getDestination() {return destination;}

    //Pickup-delivery pairs (precedence and pairing; supported by PWDefault with forward search only)
    int setPair(int pickup, int delivery);
    bool hasPairs() const {return n_pairs > 0;}
    bool isPickup(int node) const {return node >= 0 and node < (int)is_pickup.size() and is_pickup[node];}
    int getPickupOf(int node) const {return (node >= 0 and node < (int)pickup_of.size()) ? pickup_of[node] : -1;}

    //Topology flags
    bool isDirected(){return directed;}
    bool isSymmetric(){return symmetric;}
    bool isGraphCyclic(){return cycles;}

    //Memory saving information
    bool isDataCompressed(){return compress_data;}

    /** Graph management**/
    //Get distance (coordinate based)
    int getCoordDistanceType(std::string distance);
    int getCoordDistance(int i, int j, int distance_algo) {return network.getCoordDistance(i, j, distance_algo);}

    //Neighbors
    bool areNeighbors(int i, int j, bool direction) {return network.areNeighbors(i, j, direction);}
    std::vector<int> & getNeighbors(int node, bool direction) {return network.getNeighbors(node, direction);}

    //Active nodes
    bool isActiveNode(int i) {return network.isActiveNode(i);}
    int countActiveNodes() {return network.countActiveNodes();}

    void activateNode(int i) {network.activateNode(i);}

    void pruneActiveNode(int i) {network.pruneActiveNode(i);}
    void pruneUnreachableNodes(Bitset & reachable) {network.pruneUnreachableNodes(reachable);}

    void resetActiveNodes() {network.resetActiveNodes();}

    /** Objective and Resource management **/
    //Objective initialization
    void initObjective(Resource* objective = nullptr);

    //Resource management
    int addResource(std::string type);
    void createResources(std::vector<std::string> & resources_type);
    void setRes(Resource* res){resources.push_back(res);}
    void setRes(int position, Resource* res){resources[position] = res;}
    void setResources(std::vector<Resource*> resources){this->resources = resources;}

    //Resource/Objective data queries
    int getNumRes(){return resources.size();}
    Resource* getObj() {return objective;}
    Resource* getRes(int position){return resources[position];}
    std::vector<Resource*> & getResources(){return resources;}

    int getMinConsumptionAtExtension(int res_id);

    /** Completion Labels management  **/
    //Completion Labels data queries
    void initBoundLabels(){bound_labels = new BoundLabels(n_res, n_nodes);}
    BoundLabels* getBoundLabels(){return bound_labels;}
    void resetBoundLabels(){delete bound_labels; initBoundLabels();}

    /** Read from instance file **/
    //Read standard input problem and print problem data
    virtual void readProblem(std::string file_name);
    void getTokens(std::string & line, std::vector<std::string> & tokens);
    bool readNextLine(std::ifstream & f, std::string & line, std::vector<std::string> & tokens, std::string & key);

    /** Construction interface **/

    void setName(std::string name){this->name = name;}

    // Topology
    int setNumNodes(int n);
    int setOrigin(int origin);
    int setDestination(int destination);
    int setDirected(bool directed);
    int setCyclic(bool cyclic);
    int setSymmetric(bool symmetric);
    void setCoordinates(int i, int x, int y);
    void setCoordinatesType(std::string coord_distance_id, int coord_distance_type);
    int addArc(int i, int j);

    // Resources
    int setResBounds(int res_id, int lb, int ub);
    int setResNodeBound(int res_id, int node, int lb, int ub);
    int setResArcConsumption(int res_id, int i, int j, int cost);
    int setResNodeConsumption(int res_id, int i, int cost);

    // Objective
    int setBounds(double lb, double ub);
    int setArcCost(int i, int j, double cost);
    int setNodeCost(int i, double cost);
    //Set multiple values (needs testing)
    int setNodeCosts(std::vector<double> costs);
    void setArcMatrixCost(std::vector<std::vector<double>> costs);

    double getCostScaleFactor() const { return cost_scale_factor; }
    void scaleCosts();

    //Helper functions
    bool nodeExists(int node) const;
    bool resExists(int res_id) const;
    bool objExists() const;

    // Finalization
    void init();
    void build();
    bool validate();

    /** Output management **/
    void printProblem();

    /** Data collection **/
    void initDataCollection();
    void collectData();

protected:

    std::string name;
    int problem_status;

    //Graph information
    Graph network;
    bool directed;
    bool symmetric;
    bool cycles;
    bool compress_data;
    bool directed_set;
    bool symmetric_set;
    bool cyclic_set;
    bool init_done;
    bool build_done;
    bool duplicate_depot;

    //Node information
    int n_nodes;
    int origin, destination;

    //Pickup-delivery pairs: pickup_of[delivery] = pickup (-1 otherwise), is_pickup[node] = 1 for pickups
    int n_pairs;
    std::vector<char> is_pickup;
    std::vector<int> pickup_of;

    //Arc information Data
    int n_arcs;

    //Objective and Resources
    int n_res;
    Resource* objective;
    std::vector<Resource*> resources;

    //Cost scale factor (computed by scaleCosts(), used to convert int objective back to double)
    double cost_scale_factor;

    //Completion Labels
    BoundLabels* bound_labels;

    //Data collection
    DataCollector collector;

};

#endif
