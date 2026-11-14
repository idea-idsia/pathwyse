#ifndef LM_BUCKET_H
#define LM_BUCKET_H

#include <set>
#include "algorithms/labels/label_advanced.h"
#include "data/problem.h"


class LMBucket{

public:

    /** LM management **/
    //Constructors and destructors
    explicit LMBucket(Problem* problem);
    ~LMBucket() = default;

    //Initialization
    void initLM();
    void readConfiguration();

    void resetLM();

    //Support methods
    void setName(std::string name) {this->name = name; collector.setCollectionName(name);}
    void setProblem(Problem* problem) {this->problem = problem;}
    void setCompareUnreachables(bool compare_unreachables) {this->compare_unreachables = compare_unreachables;}
    void setUnreachables(std::vector<Bitset>* unreachable_active) {this->unreachable_active = unreachable_active;}
    void setUnreachablesNG(std::vector<Bitset>* unreachable_ng) {this->unreachable_ng = unreachable_ng;}
    int getCompareUnreachables(){return compare_unreachables;}

    //Bidirectional budget management
    void update_split();
    void setSplit(double split){this->split_ratio = split;}
    double getSplit() {return split_ratio;}

    //Incumbent
    cost_t getIncumbent(){return incumbent;}
    void setIncumbent(cost_t incumbent){this->incumbent = incumbent;}

    //Queries
    int getForwardSize() {return ninserted_fw - ndominated_fw;}
    int getBackwardSize() {return ninserted_bw - ndominated_bw;}
    int totalLabels(){return (ninserted_fw - ndominated_fw + ninserted_bw - ndominated_bw);}

    /** Candidate management **/
    bool candidatesAvailable(bool forward = true, bool backward = true);
    LabelAdv* getCandidate(bool forward = true, bool backward = true);
    LabelAdv* getCandidateBucket(bool forward, bool backward);

    /** Label management**/
    //Label extension
    bool isNodeReachable(LabelAdv *label, int next_node);
    bool isExtensionFeasible(LabelAdv *label, int next_node);
    bool isCriticalExtensionFeasible(LabelAdv *label, int next_node);
    void extendLabel(LabelAdv *current_label, LabelAdv *new_label, int next_node);
    void updateUnreachables(LabelAdv *label);
    void updateMaskedNeighbors();
    LabelAdv* getWorkingLabel(bool direction, int node) {return direction? &forward_tmp[node] : &backward_tmp[node];}

    //Label insertion
    LabelAdv* insert(LabelAdv *new_label);
    FORCE_INLINE bool dominates(LabelAdv *l1, LabelAdv *l2);
    NO_INLINE bool dominatesTwoCycle(LabelAdv *l1, LabelAdv *l2);

    /** Join **/
    void join();
    void naiveJoin();
    void naiveNodeJoin();
    void orderedJoin();
    void orderedNodeJoin();
    void ksol_orderedJoin();
    void ksol_orderedNodeJoin();
    void bucketNodeJoin();
    void ksol_bucketNodeJoin();

    void populateQueues();
    bool isJoinFeasible(LabelAdv* forward, LabelAdv* backward);         //Checks if two labels can be joined
    bool isNodeJoinFeasible(LabelAdv* forward, LabelAdv* backward);     //Checks if two labels can be joined

    bool joinFound() {return not joinable_labels.empty();}      //Checks if a join was found

    std::tuple<cost_t, LabelAdv*, LabelAdv*> getBestJoin() {return *joinable_labels.begin();}       //returns best join
    std::multiset<std::tuple<cost_t, LabelAdv*, LabelAdv*>> getAllJoin() {return joinable_labels;}

    /** Solution management **/
    std::tuple<cost_t, LabelAdv*, LabelAdv*> getSolutionLabels();
    int getJoinRank(LabelAdv* label_forward, LabelAdv* label_backward);
    LabelAdv* getODLabel(){return od_label;}
    void sortLabelsInQueue(bool direction, int node);
    const std::vector<LabelAdv*>& getLabelsInQueue(bool direction, int node);

    /** Debug **/
    std::list<LabelAdv> buildTour(std::list<int> tour, bool direction);
    void printBucket(int bucket_idx, int node, bool direction);

    /** Data collection management **/
    void initDataCollection();
    void initLabelDataCollection(DataCollector* collector_label);
    void setExecutionID(int id){executionID = id;}
    void setIteration(int iterations){this->iterations = iterations;}
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

    int         bucket_resource_id;
    std::string bucket_size_mode;                           // Decides the size of the bucket. Can be unitary or min res consumption
    float       prev_percentage, next_percentage;           // Percentage of overall number of buckets to check for dominance

    int requested_solutions;
    double split_ratio;
    cost_t incumbent;
    bool compare_unreachables;
    std::vector<Bitset>* unreachable_active = nullptr;
    std::vector<Bitset>* unreachable_ng = nullptr;
    std::vector<std::vector<int>> masked_neighbors_fw, masked_neighbors_bw;
    bool two_cycle_elimination;

    int ninserted_fw, ninserted_bw;
    int ndominated_fw, ndominated_bw;
    int nclosed_fw, nclosed_bw;

    //direction, index in the storage
    LabelAdv* od_label;

    /** Label pools, masks, and pointers **/
   
    std::vector<LabelAdv> forward_tmp, backward_tmp;        //Tmp Labels (for insertion)
    std::vector<LabelAdv*> forward_best, backward_best;     //Best overall LabelAdv for each node

    std::multiset<std::tuple<cost_t, LabelAdv*, LabelAdv*>> joinable_labels;   //Joinable label pairs

    // Bucket data structures

    int bucket_count;                       // Number of buckets
    int bucket_size;                        // Size of a bucket (1 or min res consumption)
    int fw_cur_bucket_idx, fw_cur_node;
    int bw_cur_bucket_idx, bw_cur_node;
    std::vector<int> min_bucket_to_check, max_bucket_to_check;          // Indexes of buckets to check for dominance

    std::vector<std::vector<std::list<LabelAdv>>> fw_bucket, bw_bucket; // Label pools (bucket)

    std::list<LabelAdv>::iterator fw_ptr_bucket, bw_ptr_bucket;         // Bucket iterators
    bool fw_ptr_unset, bw_ptr_unset;                                    // Bucket iterator initialization flag

    std::vector<std::vector<LabelAdv*>> fw_queue, bw_queue;             // Label pools (queue, for each node) for join operations

    std::vector<int> min_consumption_at_bucket, fw_nonempty, bw_nonempty; // Helpers for bucketNodeJoin

    //Data collection
    int executionID;
    int iterations;
    int ins_attempts_fw, ins_attempts_bw;
    unsigned long long joinComparisons;
    DataCollector collector;
    DataCollector collector_label_fw, collector_label_bw;

};

#endif
