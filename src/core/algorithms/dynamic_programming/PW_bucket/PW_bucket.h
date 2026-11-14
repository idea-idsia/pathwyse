#ifndef PW_BUCKET_H
#define PW_BUCKET_H

#include "algorithms/algorithm.h"
#include "algorithms/labels/label_advanced.h"
#include "algorithms/preprocessing/preprocessing.h"
#include "LM_bucket.h"

class PWBucket: public Algorithm {

public:

    /** Algorithm management **/
    //Constructors and destructors
    PWBucket(std::string name, Problem* problem);
    ~PWBucket();

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
    LMBucket* label_manager;

    //Parameters (Configuration)
    int search_direction;
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
};

#endif
