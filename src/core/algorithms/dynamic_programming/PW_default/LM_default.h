#ifndef LM_DEFAULT_H
#define LM_DEFAULT_H

#include <set>
#include "algorithms/labels/label_advanced.h"
#include "data/problem.h"

//Queue based Label Manager for PW_default.
//Handles data structures and returns candidates for the extension and join steps

//Resource scope of a feasibility check. Monotone resources never become feasible again along a route.
//Load with deliveries is not monotone.
enum {SCOPE_ALL = 0, SCOPE_MONOTONE = 1, SCOPE_NONMONOTONE = 2};

class LMDefault{

public:

    /** LM management **/
    //Constructors and destructors
    explicit LMDefault(Problem* problem);
    ~LMDefault() = default;

    //Initialization
    void initLM();
    void readConfiguration();

    void resetLM();

    //Support methods
    void setName(std::string name) {this->name = name; collector.setCollectionName(name);}
    void setProblem(Problem* problem) {this->problem = problem;}
    void setUseVisited(bool use_visited) {this->use_visited = use_visited;}
    void setCompareUnreachables(bool compare_unreachables) {this->compare_unreachables = compare_unreachables;}
    void setForwardSearch() {search_direction = SEARCH_FORWARD; bidirectional = false;}
    int getUseVisited(){return use_visited;}
    int getCompareUnreachables(){return compare_unreachables;}

    //Bidirectional budget management
    void update_split();
    void setSplit(double split){this->split_ratio = split;}
    double getSplit() {return split_ratio;}

    //Incumbent
    int getIncumbent(){return incumbent;}
    void setIncumbent(int incumbent){this->incumbent = incumbent;}

    //Queries
    int getForwardSize() {return forward_labels.size();}
    int getBackwardSize() {return backward_labels.size();}
    int totalLabels(){return (forward_labels.size() + backward_labels.size());}
    int candidateSize(bool direction, int id) {return direction ? forward_candidates[id].size() : backward_candidates[id].size();}
    int closedSize(bool direction, int id) {return direction ? forward_closed[id].size() : backward_closed[id].size();}
    LabelAdv* getLabel(bool direction, int index) {return direction? & forward_labels[index] : & backward_labels[index];}

    /** Candidate management **/
    bool candidatesAvailable(bool forward = true, bool backward = true);
    LabelAdv* getCandidate(bool forward = true, bool backward = true);
    LabelAdv* getCandidateRR(bool forward, bool backward);
    LabelAdv* getCandidateNode(bool forward, bool backward);

    /** Label management**/
    //Label extension
    bool isNodeReachable(LabelAdv *label, int next_node);
    bool isExtensionFeasible(LabelAdv *label, int next_node, int scope = SCOPE_ALL);
    bool isPairingFeasible(LabelAdv *label, int next_node);
    bool isCriticalExtensionFeasible(LabelAdv *label, int next_node);
    void extendLabel(LabelAdv *current_label, LabelAdv *new_label, int next_node);
    void updateUnreachables(LabelAdv *label);

    //Label insertion
    LabelAdv* insert(LabelAdv *new_label);
    bool dominates(LabelAdv *l1, LabelAdv *l2);

    /** Join **/
    void join();
    void closeLabels();
    void restoreClosedLabels();
    void naiveJoin();
    void classicJoin();
    void orderedJoin();
    void orderedNodeJoin();
    void ksol_orderedJoin();
    void ksol_orderedNodeJoin();
    void paretoArcJoin();
    void paretoNodeJoin();
    bool isJoinFeasible(LabelAdv* forward, LabelAdv* backward);                 //Checks if two labels can be joined
    bool isNodeJoinFeasible(LabelAdv* forward, LabelAdv* backward);             //Checks if two labels can be joined

    void paretoSearchBlock(Resource* objective, int node_fw, int node_bw,
                           std::vector<std::pair<int, int>>& rows,
                           std::vector<std::pair<int, int>>& cols,
                           int start_row, int end_row, int start_col, int end_col);
    bool joinFound() {return not joinable_labels.empty();}                      //Checks if a join was found

    std::tuple<int, LabelAdv*, LabelAdv*> getBestJoin() {return *joinable_labels.begin();}       //returns best join
    std::multiset<std::tuple<int, LabelAdv*, LabelAdv*>> getAllJoin() {return joinable_labels;}

    /** Solution management **/
    std::tuple<int, LabelAdv*, LabelAdv*> getSolutionLabels();
    void setODLabel();
    LabelAdv* getODLabel(){return (od_label.second >= 0) ? getLabel(od_label.first, od_label.second) : nullptr;}
    void sortClosedLabels(bool direction, int node);
    const std::vector<std::pair<int, int>>& getClosedLabels(bool direction, int node);

    /** Heuristic management **/
    void setQueueLimit(int queue_limit){if(queue_limit > 0) this->queue_limit = queue_limit;}

    /** Debug **/
    std::list<LabelAdv> buildTour(std::list<int> tour, bool direction);
    void findLabels(std::list<LabelAdv> * tourLabels);
    void printStepConsumption(std::list<LabelAdv> *tourFW, std::list<LabelAdv> *tourBW);
    void printClosed(int id, bool direction = true);
    void printCandidates(int id, bool direction = true);

    /** Data collection management **/
    void initDataCollection();
    void initLabelDataCollection(DataCollector* collector_label);
    void setExecutionID(int id){executionID = id;}
    void setIteration(int iterations){this->iterations = iterations;}
    float getMeanLabels(bool direction);
    int getMaxLabels(bool direction);
    float getVarLabels(bool direction, float mean);
    unsigned long long getJoinComparisons(){return joinComparisons;}
    void collectData();
    void collectLabel(DataCollector* collector_label, LabelAdv *l);
    void writeData() {collector.writeData();  collector_label_fw.writeData(); collector_label_bw.writeData();}


protected:

    std::string name;
    std::string lm_type;

    //Problem
    Problem* problem;

    //Parameters
    bool bidirectional;
    int search_direction;
    bool autoconfiguration;

    int candidate_type;
    int join_type;
    int join_algo;

    int queue_limit;
    int reserve_size;
    int requested_solutions;
    int turn_forward, turn_backward;
    double split_ratio;
    int incumbent;
    bool use_visited, compare_unreachables;
    std::vector<char> res_monotone;                 //res_monotone[id] = 1 if resource id has no negative node consumption
    void computeMonotoneResources();
    bool two_cycle_elimination;

    int ndominated_fw, ndominated_bw;
    int nclosed_fw, nclosed_bw;

    //direction, index in the storage
    std::pair<bool, int> od_label;

    /** Label pools, masks, and pointers **/
    std::vector<LabelAdv> forward_labels, backward_labels;  //Pool of all available forward (resp. backward) labels

    //For each node, store an ordered list of labels (pointer)
    //Open labels
    std::vector<std::list<std::pair<int, int>>> forward_candidates, backward_candidates;            //Open labels
    LabelAdv *best_fw, *best_bw;                                                                    //Best overall labels
    std::vector<LabelAdv*> forward_best, backward_best;                                             //Best overall LabelAdv for each node

    //Closed labels
    std::vector<std::vector<std::pair<int, int>>> forward_closed, backward_closed;                      //Closed labels
    std::vector<std::vector<std::pair<int, int>>> forward_closed_backup, backward_closed_backup;        //Backup of closed labels

    std::multiset<std::tuple<int, LabelAdv*, LabelAdv*>> joinable_labels;                 //Joinable label pairs

    //Data collection
    int executionID;
    int iterations;
    int ins_attempts_fw, ins_attempts_bw;
    unsigned long long joinComparisons;
    DataCollector collector;
    DataCollector collector_label_fw, collector_label_bw;
};

#endif

