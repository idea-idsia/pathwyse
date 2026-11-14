#ifndef LM_ACYCLIC_H
#define LM_ACYCLIC_H

#include "algorithms/labels/label.h"
#include "data/problem.h"
#include <algorithm>

//Bucket based Label Manager for PW_acyclic.

class LMacyclic {

public:

    /** LM management **/
    //Constructors and destructors
    LMacyclic(Problem* problem);
    ~LMacyclic() {resetLM();}

    void initLM();
    void resetLM();

    //Support methods
    void setName(std::string name) {this->name = name; collector.setCollectionName(name);}

    cost_t getBucketValueF() {return pos_f + offset_f;}
    cost_t getBucketValueB() {return pos_b + offset_b;}

    void setIncumbent(cost_t incumbent) {this->incumbent = incumbent;}
    cost_t getIncumbent(){return incumbent;}

    void eraseCandidate(Label* candidate) {delete candidate;}
    void closeLabel(Label* current);

    void setSplit(double split){split_ratio = split;}

    std::list<Label> buildTour(std::list<int> tour, bool direction);

    /** Next Candidate Management **/
    bool candidatesAvailable();
    Label* getCandidate();

    /** Label management **/
    void extendLabel(Label* current_label, Label & new_label, int next_node);
    bool insert(Label& candidate);
    bool isDominated(Label* candidate);
    bool isExtensionFeasible(Label* candidate);
    bool earlyUpdate(Label* candidate);

    /** Join management **/
    void join(Label* current_label);
    std::tuple<cost_t, Label, Label>* getJoin() {return &best_label_pair;}       //returns best join

    /** Data collection management **/
    void initDataCollection();
    void setExecutionID(int id){executionID = id;}
    void collectData();
    void writeData() {collector.writeData();}

private:

    //Problem Data
    Problem* problem;
    CostResource* obj;
    Resource* res;

    //Buckets (one per objective value)
    std::vector<std::list<Label*>> bucket_f, bucket_b;
    std::vector<std::list<Label*>> closed_f, closed_b;
    int pos_f, pos_b;
    cost_t offset_f, offset_b;

    //Split
    double split_ratio;

    //Single resource only values
    std::vector<int> min_consumption_f, min_consumption_b;

    //Solution Data
    cost_t incumbent;
    std::tuple<cost_t, Label, Label> best_label_pair;

    //Completion labels
    BoundLabels* bound_labels;

    //Data Collection
    std::string name;
    std::string lm_type;
    int executionID;
    unsigned long long joinComparisons;
    int nlabels_bw, nlabels_fw, nclosed_fw, nclosed_bw, earlyImprov;
    DataCollector collector;
};


#endif
