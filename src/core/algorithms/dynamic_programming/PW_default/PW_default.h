#ifndef PW_DEFAULT_H
#define PW_DEFAULT_H

#include "algorithms/algorithm.h"
#include "algorithms/labels/label_advanced.h"
#include "algorithms/preprocessing/preprocessing.h"
#include "LM_default.h"

class PWDefault: public Algorithm {

public:

    /** Algorithm management **/
    //Constructors and destructors
    PWDefault(std::string name, Problem* problem);
    ~PWDefault();

    //Init and reset
    void initAlgorithm();
    void readConfiguration();

    void resetIteration();
    void resetAlgorithm(int reset_level);

    //Solve
    bool preprocessing();
    void solve() override;
    void labeling(bool forward, bool backward);
    void extend(LabelAdv* candidate);
    bool checkTermination();

    //Statistics of the last solve: labels created over all DSSR rounds, number of rounds
    unsigned long long getLabelCount() override {return total_labels;}
    int getRounds() override {return rounds;}

    //Path building
    void managePaths();
    void manageAdditionalPaths();

    /** Relaxation management **/
    void buildNG();            //build NG sets
    bool DssrStandard();                //DSSR
    bool DssrRestricted();     //iteratively DSSR forbid only repeating customers in cycles
    bool NgRestricted();       //iteratively forbid only cycles that are not part of an NG route
    bool isNGCompliant(Path& path);
    std::string getRelaxationName();

    /** Debug **/
    void preGenLabels(std::list<int> tour, bool direction);

    /** Data Collection management **/
    void initDataCollection();
    void collectData();
    void writeData();

protected:

    //Preprocessing (for acyclic graph)
    Preprocessing* preprocess;

    //Label Manager
    LMDefault* label_manager;

    //Parameters (Configuration)
    int search_direction;
    bool use_visited;           //check
    bool two_cycle_elimination;
    int requested_solutions;
    int dssr;                   //DSSR mode
    int ng;                     //NG mode
    int ng_size;
    std::string ng_set_path;
    bool earlyjoin;
    unsigned long long int earlyjoin_step;

    //Unreachable nodes
    std::vector<Bitset> unreachable_active;
    std::vector<Bitset> unreachable_ng;

    //Parameters (Data collection)
    int unreachable_max_count, previous_unreachable_max_count;
    bool ng_compliant;
    int it_ext_fw, it_ext_bw;
    unsigned long long total_labels = 0;
    int rounds = 0;
};

#endif
