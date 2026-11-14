#include "LM_bucket.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include "utils/logger.h"

/** LM management **/
//Constructors and destructors
LMBucket::LMBucket(Problem* problem) {
    executionID = 0;
    name = "label_manager";
    lm_type = "bucket";
    this->problem = problem;

    readConfiguration();

    collector = DataCollector(name);
    collector_label_fw = DataCollector("Labels_fw");
    collector_label_bw = DataCollector("Labels_bw");

    initDataCollection();

    iterations = 0;
    ins_attempts_fw = ins_attempts_bw = 0;
}


//Init
void LMBucket::initLM(){
    //Get data from problem
    auto objective = problem->getObj();
    std::vector<Resource*>& resources = problem->getResources();

    int origin = problem->getOrigin();
    int destination = problem->getDestination();
    int n_nodes = problem->getNumNodes();
    int n_res = problem->getNumRes();

    ninserted_fw = ninserted_bw = 0;
    ndominated_fw = ndominated_bw = 0;
    nclosed_fw = nclosed_bw = 0;

    //Data structures initialization
    forward_best.resize(n_nodes, nullptr);
    backward_best.resize(n_nodes, nullptr);

    forward_tmp = std::vector<LabelAdv>(n_nodes);
    backward_tmp = std::vector<LabelAdv>(n_nodes);

    bucket_size = (bucket_size_mode == "unit_size") ? 1 : problem->getMinConsumptionAtExtension(bucket_resource_id);
    int resUB = problem->getRes(bucket_resource_id)->getUB();
    bucket_count = (resUB / bucket_size) + 1;

    bool forward_ok = search_direction == SEARCH_BIDIRECTIONAL or search_direction == SEARCH_FORWARD;
    bool backward_ok = search_direction == SEARCH_BIDIRECTIONAL or search_direction == SEARCH_BACKWARD;

    if (forward_ok){
        fw_bucket.resize(bucket_count);
        for (int i = 0; i != bucket_count; ++i)
            fw_bucket[i].resize(n_nodes);
        fw_queue.resize(n_nodes, std::vector<LabelAdv*>());
    }

    if(backward_ok) {
        bw_bucket.resize(bucket_count); 
        for (int i = 0; i != bucket_count; ++i) 
            bw_bucket[i].resize(n_nodes);
        bw_queue.resize(n_nodes, std::vector<LabelAdv*>());
    }

    if (iterations == 1)
    {
        // For each bucket, compute the range of valid buckets for dominance
        min_bucket_to_check.resize(bucket_count,0);
        max_bucket_to_check.resize(bucket_count, bucket_count);

        int shift_prev = static_cast<int>(std::ceil(prev_percentage * bucket_count));
        int shift_next = static_cast<int>(std::ceil(next_percentage * bucket_count));

        for (int i = 0; i != bucket_count; ++i)
        {
            if (prev_percentage <= 0.0) 
                min_bucket_to_check[i] = i;
            else if (prev_percentage < 1.0) 
                min_bucket_to_check[i] = std::max(0,i-shift_prev); // included in check
            

            if (next_percentage <= 0.0)
                max_bucket_to_check[i] = i;
            else if (next_percentage < 1.0)
                max_bucket_to_check[i] = std::min(bucket_count,i+shift_next); // excluded from check
        }

        if (join_algo == JOIN_BUCKET_NODE or join_algo == JOIN_KBUCKET_NODE){
            min_consumption_at_bucket.resize(bucket_count);
            for (int i = 0; i != bucket_count; ++i)
                min_consumption_at_bucket[i] = i*bucket_size;
        }
    }

    //Push back first forward and backward labels
    bool direction = true;
    LabelAdv* predecessor = nullptr;

    //Initialize snapshots
    LabelAdv* initial_label_fw = getWorkingLabel(true, origin);
    LabelAdv* initial_label_bw = getWorkingLabel(false, destination);

    if (forward_ok) {
        initial_label_fw->initLabel(origin, predecessor, direction, n_res);
        initial_label_fw->initVisited(origin, n_nodes);
        initial_label_fw->setObjective(objective->getInitValue() + objective->getNodeCost(origin));
        ++ins_attempts_fw;
    }
    if(backward_ok) {
        initial_label_bw->initLabel(destination, predecessor, !direction, n_res);
        initial_label_bw->initVisited(destination, n_nodes);
        initial_label_bw->setObjective(objective->getInitValue() + objective->getNodeCost(destination));
        ++ins_attempts_bw;
    }

    for(int id = 0; id < problem->getNumRes(); id++) {
        if (forward_ok) initial_label_fw->setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(origin));
        if (backward_ok) initial_label_bw->setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(destination)); // initValue needed?
    }

    //Insert labels in the buckets
    if (forward_ok){
        fw_cur_bucket_idx = initial_label_fw->getSnapshot(bucket_resource_id) / bucket_size;
        fw_bucket[fw_cur_bucket_idx][origin].emplace_back(*initial_label_fw);
        ++ninserted_fw;
        forward_best[origin] = &fw_bucket[fw_cur_bucket_idx][origin].back();

    }
    if(backward_ok){
        bw_cur_bucket_idx = initial_label_bw->getSnapshot(bucket_resource_id) / bucket_size;
        bw_bucket[bw_cur_bucket_idx][destination].emplace_back(*initial_label_bw);
        ++ninserted_bw;
        backward_best[destination] = &bw_bucket[bw_cur_bucket_idx][destination].back();

    }

    fw_ptr_unset = bw_ptr_unset = true;
    fw_cur_bucket_idx = bw_cur_bucket_idx = 0;
    fw_cur_node = bw_cur_node = 0;

    incumbent = MAX_COST;
    od_label = nullptr;
    joinComparisons = 0;

}

void LMBucket::readConfiguration() {
    search_direction = Parameters::getDefaultSearch();
    bidirectional = search_direction == SEARCH_BIDIRECTIONAL;
    autoconfiguration = Parameters::isDefaultAutoConfigured();
    split_ratio = Parameters::getDefaultSplit();
    compare_unreachables = true;
    two_cycle_elimination = Parameters::useTwoCycleElimination() and compare_unreachables;
    candidate_type = Parameters::getDefaultCandidateType();
    join_algo = Parameters::getDefaultJoinType();
    join_type = (
        join_algo == JOIN_ORDERED_NODE or 
        join_algo == JOIN_KORDERED_NODE or 
        join_algo == JOIN_NAIVE_NODE or 
        join_algo == JOIN_BUCKET_NODE or 
        join_algo == JOIN_KBUCKET_NODE
    ) ? JOIN_NODE : JOIN_ARC;
    requested_solutions = Parameters::getRequestedSolutions();

    if(autoconfiguration) {
        if (bidirectional and requested_solutions > 1){
            if (join_type == JOIN_NODE and join_algo != JOIN_KORDERED_NODE and join_algo != JOIN_KBUCKET_NODE)
                join_algo = JOIN_KBUCKET_NODE;
            else if (join_type == JOIN_ARC)
                join_algo = JOIN_KORDERED;
        }
    }

    bucket_resource_id = Parameters::getBucketResource();

    std::vector<Resource*>& resources = problem->getResources();
    if (bucket_resource_id < 0 or bucket_resource_id >= problem->getNumRes() or not resources[bucket_resource_id]->isBucketCompatible()){
        bool not_found = true;
        for(int id = 0; id < problem->getNumRes(); id++) {
            if (resources[id]->isBucketCompatible()){
                Logger::warn("Resource " + std::to_string(bucket_resource_id) + " is not suitable for building the bucket. Using resource " + std::to_string(id) + ".");
                bucket_resource_id = id;
                not_found = false;
                break;
            }
        }
        if (not_found){
            Logger::error("Resource " + std::to_string(bucket_resource_id) + " is not suitable for building the bucket. No other suitable resource found.");
            exit(EXIT_FAILURE);
        }
    }

    bucket_size_mode = Parameters::getBucketSizeMode();
    prev_percentage = Parameters::getBucketPrevDomCheckPercentage();
    next_percentage = Parameters::getBucketNextDomCheckPercentage();
}

void LMBucket::resetLM(){

    forward_best.clear();
    backward_best.clear();

    fw_bucket.clear();
    bw_bucket.clear();
    fw_queue.clear();
    bw_queue.clear();

    joinable_labels.clear();
    incumbent = MAX_COST;
    collector_label_fw.clearSubsetRecords();
    collector_label_bw.clearSubsetRecords();
    ins_attempts_fw = ins_attempts_bw = 0;
}

void LMBucket::update_split(){
    if(Parameters::isCollecting())
        collector.collect("split", split_ratio);

    int f_size = nclosed_fw, b_size = nclosed_bw;

    if(f_size - b_size + f_size*0.2 < 0)
        split_ratio += 0.05;
    else if(b_size - f_size + b_size*0.2 < 0)
        split_ratio -= 0.05;
}

/** Candidate management **/
//Returns true if an open label is available
bool LMBucket::candidatesAvailable(bool forward, bool backward) {

    if(forward and ndominated_fw + nclosed_fw < ninserted_fw)
        return true;
    if(backward and ndominated_bw + nclosed_bw < ninserted_bw)
        return true;

    return false;

    /*
    //Safe (but slower) method
    int starting_bucket = 0;
    if (forward and backward)   starting_bucket = std::min(fw_cur_bucket_idx,bw_cur_bucket_idx);
    else if (forward)           starting_bucket = fw_cur_bucket_idx;
    else if (backward)          starting_bucket = bw_cur_bucket_idx;

    for(int r = starting_bucket; r < bucket_count; r++) {
        for (int n = 0; n < problem->getNumNodes(); n++){
            if(forward and not fw_bucket[r][n].empty())
                return true;
            if(backward and not bw_bucket[r][n].empty())
                return true;
        }
    }
    return false;
    */
}

//Returns an open candidate
LabelAdv* LMBucket::getCandidate(bool forward, bool backward) {

    LabelAdv* candidate = getCandidateBucket(forward, backward);

    if(candidate != nullptr)
        candidate->getDirection() ? nclosed_fw++ : nclosed_bw++;

    return candidate;
}

//Returns an open label from the first non-empty bucket
LabelAdv* LMBucket::getCandidateBucket(bool forward, bool backward) {

    LabelAdv* candidate = nullptr;
    
    int origin = problem->getOrigin();
    int destination = problem->getDestination();
    int n_nodes = problem->getNumNodes();

    bool direction = forward; 

    auto &cur_bucket_idx    = direction ? fw_cur_bucket_idx : bw_cur_bucket_idx;
    auto &cur_node          = direction ? fw_cur_node : bw_cur_node;

    while (cur_bucket_idx != bucket_count)
    {
        while (cur_node != n_nodes)
        {
            if (forward and cur_node != destination)
            {
                if (fw_ptr_unset)
                {
                    fw_ptr_bucket = fw_bucket[cur_bucket_idx][cur_node].begin();
                    fw_ptr_unset = false;
                }
                while (fw_ptr_bucket != fw_bucket[cur_bucket_idx][cur_node].end())
                {
                    candidate = &(*fw_ptr_bucket);
                    ++fw_ptr_bucket;
                    return candidate;
                }
            }

            if (backward and cur_node != origin)
            {
                if (bw_ptr_unset)
                {
                    bw_ptr_bucket = bw_bucket[cur_bucket_idx][cur_node].begin();
                    bw_ptr_unset = false;
                }
                while (bw_ptr_bucket != bw_bucket[cur_bucket_idx][cur_node].end())
                {
                    candidate = &(*bw_ptr_bucket);
                    ++bw_ptr_bucket;
                    return candidate;
                }
            }

            if (forward){
                fw_ptr_unset = true;
                ++fw_cur_node;
            }
            if (backward){
                bw_ptr_unset = true;
                ++bw_cur_node;
            }
        }

        if (forward){
            fw_cur_node = 0;
            ++fw_cur_bucket_idx;
        }
        
        if (backward){
            bw_cur_node = 0;
            ++bw_cur_bucket_idx;
        }
    }

    Logger::error("LMBucket::getCandidateBucket : no candidate available!");
    exit(EXIT_FAILURE);
}

/** Label management **/
//Extension
bool LMBucket::isNodeReachable(LabelAdv *label, int next_node){
    return label->isReachable(next_node);
}

bool LMBucket::isExtensionFeasible(LabelAdv *label, int next_node) {
    int current_value;
    bool direction = label->getDirection();
    double bounding = 1.;

    std::vector<Resource*>& resources = problem->getResources();

    int i = direction ? label->getNode() : next_node;
    int j = direction ? next_node : label->getNode();

    for(int id = 0; id < resources.size(); id++) {
        current_value = label->getSnapshot(id);
        current_value = resources[id]->extend(current_value, i, j, direction);
        if(not resources[id]->isFeasible(current_value, next_node, bounding, direction))
            return false;
    }

    return true;
}

bool LMBucket::isCriticalExtensionFeasible(LabelAdv *label, int next_node) {
    int current_value;
    bool direction = label->getDirection();
    double bounding = direction ? split_ratio : 1 - split_ratio;

    std::vector<Resource*>& resources = problem->getResources();

    int i = direction ? label->getNode() : next_node;
    int j = direction ? next_node : label->getNode();

    current_value = label->getSnapshot(RES_CRITICAL);

    if(join_type == JOIN_ARC and direction)
        current_value = resources[RES_CRITICAL]->extend(current_value, i, j, direction);

    if(not resources[RES_CRITICAL]->isFeasible(current_value, next_node, bounding, direction))
        return false;

    return true;
}

void LMBucket::extendLabel(LabelAdv *current_label, LabelAdv *new_label, int next_node) {
    auto objective = problem->getObj();
    std::vector<Resource*>& resources = problem->getResources();
    *new_label = *current_label;
    new_label->updateLabel(next_node, current_label);
    bool direction = current_label->getDirection();

    int i = direction ? current_label->getNode() : next_node;
    int j = direction ? next_node : current_label->getNode();

    //Update Objective
    cost_t current_obj, new_obj;
    current_obj = current_label->getObjective();
    new_obj = objective->extend(current_obj, i, j, direction);
    new_label->setObjective(new_obj);

    //Update resources
    int current_value, new_value;
    for(int id = 0; id < problem->getNumRes(); id++) {
        current_value = current_label->getSnapshot(id);
        new_value = resources[id]->extend(current_value, i, j, direction);
        new_label->setSnapshot(id, new_value);
    }

}

void LMBucket::updateUnreachables(LabelAdv *candidate) {
    int predecessor = candidate->getPredecessorNode();
    short label_type = PARTIALLY_DOMINANT;
    int node = candidate->getNode();
    bool direction = candidate->getDirection();
    std::vector<int> & neighbors = direction ? masked_neighbors_fw[node] : masked_neighbors_bw[node];
    for(auto & neigh: neighbors)
        if(candidate->isReachable(neigh) and not isExtensionFeasible(candidate, neigh))
            candidate->setUnreachable(neigh);

    if (two_cycle_elimination and
        (predecessor == (direction ? problem->getOrigin() : problem->getDestination())
         or not problem->isActiveNode(predecessor)
         or not problem->areNeighbors(node, predecessor, direction)
         or ((*unreachable_active)[node].get(predecessor) and not candidate->isReachable(predecessor))
         or not isExtensionFeasible(candidate, predecessor)))
        label_type = DOMINANT;

    candidate->setLabelType(label_type);
}

void LMBucket::updateMaskedNeighbors() {
    masked_neighbors_fw.assign(problem->getNumNodes(), {});
    masked_neighbors_bw.assign(problem->getNumNodes(), {});
    if(not unreachable_active or unreachable_active->empty())
        return;

    for(int node = 0; node < problem->getNumNodes(); node++) {
        Bitset & mask = (*unreachable_active)[node];
        for(int neigh: problem->getNeighbors(node, true))
            if(mask.get(neigh))
                masked_neighbors_fw[node].push_back(neigh);
        for(int neigh: problem->getNeighbors(node, false))
            if(mask.get(neigh))
                masked_neighbors_bw[node].push_back(neigh);
    }
}

//Insertion
//Objective based insert
LabelAdv* LMBucket::insert(LabelAdv* new_label) {
    new_label->getDirection() ? ++ins_attempts_fw : ++ins_attempts_bw;

    DataCollector* collector_label = new_label->getDirection() ? & collector_label_fw : & collector_label_bw;

    //Checks if new label is suboptimal
    if(not problem->isGraphCyclic() and new_label->getObjective() > incumbent)
        return nullptr;

    const int origin = problem->getOrigin();
    const int destination = problem->getDestination();
    const int node = new_label->getNode();
    const bool direction = new_label->getDirection();

    const cost_t objective = new_label->getObjective();

    const int bucket_idx = new_label->getSnapshot(bucket_resource_id) / bucket_size;

    auto & bucket = direction ? fw_bucket : bw_bucket;
    auto & best = direction ? forward_best : backward_best;
    auto & ninserted = direction ? ninserted_fw : ninserted_bw;
    auto & ndominated = direction ? ndominated_fw : ndominated_bw;
    auto & nclosed = direction ? nclosed_fw : nclosed_bw;

    LabelAdv* old_label;

    // Dominance in the bucket of equal consumption
    // (full checks)
    
    auto it = bucket[bucket_idx][node].begin();
    while (it != bucket[bucket_idx][node].end() and it->getObjective() < objective)
    {
        old_label = &(*it);
        if (dominates(old_label, new_label))
            return nullptr;
        ++it;
    }
    std::list<LabelAdv>::iterator insert_position;
    bool any_label_with_lower_obj = (it != bucket[bucket_idx][node].begin());
    if (any_label_with_lower_obj)
        insert_position = std::prev(it);
    while (it != bucket[bucket_idx][node].end() and it->getObjective() == objective)
    {
        old_label = &(*it);
        if (dominates(old_label, new_label))
            return nullptr;
        if (dominates(new_label, old_label))
        {
            ndominated++;
            if ((direction and node == destination) or (not direction and node == origin))
                nclosed--;
            it = bucket[bucket_idx][node].erase(it);
        }
        else
            ++it;
    }
    while (it != bucket[bucket_idx][node].end())
    {
        old_label = &(*it);
        if (dominates(new_label, old_label))
        {
            ndominated++;
            if ((direction and node == destination) or (not direction and node == origin))
                nclosed--;
            it = bucket[bucket_idx][node].erase(it);
        }
        else
            ++it;
    }
    if (any_label_with_lower_obj)   insert_position = std::next(insert_position);
    else                            insert_position = bucket[bucket_idx][node].begin();
    
    // Dominance in the buckets of lower consumption
    // (check if new label is dominated)

    for (auto r = bucket_idx-1; r >= min_bucket_to_check[bucket_idx]; --r)
    {
        for (auto it = bucket[r][node].begin(); it != bucket[r][node].end(); ++it)
        {
            old_label = &(*it);
            if (old_label->getObjective() > objective) break;
            if (dominates(old_label, new_label))
                return nullptr;
        }
    }

    // Insert label
    auto new_label_it = bucket[bucket_idx][node].insert(insert_position, *new_label);
    ++ninserted;

    LabelAdv* new_label_ins = &(*new_label_it);

    if(Parameters::getCollectionLevel() >= 4 and Parameters::isOutputStored()) {
        collectLabel(collector_label, new_label_ins);
        collector_label->markLastRecord(false);
        collector_label->addLastRecordToSubset();
    }

    //Keep track of the lowest cost label at each node
    if(not best[node] or best[node]->getObjective() >= objective)
        best[node] = new_label_ins;

    //Add it to either closed or open labels
    if ((direction and node == destination) or (not direction and node == origin)){
        nclosed++;
    }
    else {

        // Dominance in the buckets of higher consumption
        // (check if new label dominates an older one)

        for (auto r = bucket_idx+1; r < max_bucket_to_check[bucket_idx]; ++r)
        {
            for (auto it = bucket[r][node].begin(); it != bucket[r][node].end();)
            {
                old_label = &(*it);
                if (dominates(new_label_ins, old_label)) {
                    ndominated++;
                    if ((direction and node == destination) or (not direction and node == origin))
                        nclosed--;
                    it = bucket[r][node].erase(it);
                } else
                    ++it;  // only increment if not erased
            }
        }
    }

    //Update incumbent
    if((node == origin or node == destination) and objective <= incumbent) {
        incumbent = objective;
        od_label = new_label_ins;
    }

    return new_label_ins;
}

//Returns true if l1 dominates l2
bool LMBucket::dominates(LabelAdv* l1, LabelAdv* l2)  {

    if(l1->getObjective() > l2->getObjective())
        return false;

    for(int id = 0; id < l1->getSnapshot().size(); id++)
        if(l1->getSnapshot(id) > l2->getSnapshot(id))
            return false;

    if(not compare_unreachables)
        return true;

    if (l1->getUnreachableCount() > l2->getUnreachableCount())
        return false;

    //Unreachable dominance
    if (not is_subset(l1->getUnreachable(), l2->getUnreachable()))
        return false;

    if (not two_cycle_elimination)
        return true;

    return dominatesTwoCycle(l1, l2);
}

bool LMBucket::dominatesTwoCycle(LabelAdv* l1, LabelAdv* l2)  {
    //if L1 is dominant, L2 can be discarded
    if (l1->getLabelType() == DOMINANT)
        return true;

    //if L1 is dominated, L2 can be discarded (i.e., a partially dominant label exists that also dominates L2)
    if (l1->getLabelType() == DOMINATED)
        return l1->getExtensionTarget() != l2->getPredecessorNode();

    //Otherwise, L1 is partially dominant
    int l1_predecessor = l1->getPredecessorNode();
    int l2_predecessor = l2->getPredecessorNode();

    //if L1 and L2 have the same predecessor, L2 can be discarded
    if (l1_predecessor==l2_predecessor)
        return true;

    //if L2 is dominated and will not extend towards L1 predecessor, L2 can be discarded
    //Otherwise, L2 cannot be discarded
    if(l2->getLabelType() == DOMINATED)
        return l2->getExtensionTarget() != l1_predecessor;

    //if there is no arc from L2 node to L1 predecessor, then L2 can be discarded
    if (not problem->areNeighbors(l2->getNode(), l1_predecessor, l2->getDirection()))
        return true;

    //if L2 does not have resources to reach L1 predecessor, then L2 can be discarded
    if (not isExtensionFeasible(l2, l1_predecessor))
        return true;

    //Otherwise, L2 is dominated but not discarded
    //L2 will only extend towards l1 predecessor
    l2->setLabelType(DOMINATED);
    l2->setExtensionTarget(l1_predecessor);
    return false;
}

/** Join **/
void LMBucket::join(){
    Logger::debug("Joining...");

    switch(join_algo){
        case JOIN_NAIVE:
            naiveJoin();
            break;
        case JOIN_NAIVE_NODE:
            naiveNodeJoin();
            break;
        case JOIN_ORDERED:
            orderedJoin();
            break;
        case JOIN_ORDERED_NODE:
            orderedNodeJoin();
            break;
        case JOIN_KORDERED:
            ksol_orderedJoin();
            break;
        case JOIN_KORDERED_NODE:
            ksol_orderedNodeJoin();
            break;
        case JOIN_BUCKET_NODE:
            bucketNodeJoin();
            break;
        case JOIN_KBUCKET_NODE:
            ksol_bucketNodeJoin();
            break;
        default:
            if (join_type == JOIN_NODE) orderedNodeJoin();
            else orderedJoin();
            break;
    }
}

void LMBucket::naiveJoin(){
    auto objective = problem->getObj();
    cost_t cost;
    joinComparisons = 0;

    populateQueues();

    for (int i = 0; i < fw_queue.size(); i++)
        if(i != problem->getDestination())
            for(int j = 0; j < bw_queue.size(); j++)
                if(j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    for (auto &label_forward : fw_queue[i]){
                        for (auto &label_backward: bw_queue[j]) {
                            joinComparisons++;
                            cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), i, j);
                            if(cost <= incumbent and
                               isJoinFeasible(label_forward, label_backward)) {
                                incumbent = cost;
                                joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                            }
                        }
                    }
                }
}

void LMBucket::naiveNodeJoin(){
    auto objective = problem->getObj();
    cost_t cost;
    joinComparisons = 0;

    populateQueues();

    //Perform join, on every node
    for (int node = 0; node < problem->getNumNodes(); node++)
        for (auto &label_fw : fw_queue[node])
            for (auto &label_bw : bw_queue[node]){
                joinComparisons++;
                cost = objective->join(label_fw->getObjective(), label_bw->getObjective(), node);
                if (cost <= incumbent and isNodeJoinFeasible(label_fw, label_bw)){
                    incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_fw, label_bw));
                }
            }
}

void LMBucket::orderedJoin(){
    auto objective = problem->getObj();
    std::multiset<std::tuple<cost_t, int, int>> orderedPairs;
    cost_t cost;
    int i, j;

    populateQueues();

    // Sort queues by objective
    for(int p = 0; p < fw_queue.size(); p++){
        if(not fw_queue[p].empty())
            std::sort(
                fw_queue[p].begin(),
                fw_queue[p].end(),
                [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
        if(not bw_queue[p].empty())
            std::sort(
                bw_queue[p].begin(),
                bw_queue[p].end(),
                [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
    }

    //Prepare data structure
    for(i = 0; i < fw_queue.size(); i++)
        if(not fw_queue[i].empty() and i != problem->getDestination())
            for(j = 0; j < bw_queue.size(); j++)
                if(not bw_queue[j].empty() and j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    cost = objective->join(forward_best[i]->getObjective(), backward_best[j]->getObjective(), i, j);
                    if(cost <= incumbent)
                        orderedPairs.insert(std::make_tuple(cost, i, j));
                }

    //attempt join
    joinComparisons = 0;
    while(not orderedPairs.empty()){
        cost = std::get<0>(*orderedPairs.begin());
        if(cost <= incumbent){
            i = std::get<1>(*orderedPairs.begin());
            j = std::get<2>(*orderedPairs.begin());
            orderedPairs.erase(orderedPairs.begin());
            for(auto &label_forward : fw_queue[i]) {
                //If current label (i) +  best label of j is worse than bound, skip to next i-j combination
                if(objective->join(label_forward->getObjective(), backward_best[j]->getObjective(), i, j) <= incumbent) {
                    for (auto &label_backward: bw_queue[j]){
                        cost = objective->join(label_forward->getObjective(), label_backward->getObjective() , i, j);
                        if (cost <= incumbent){
                            joinComparisons++;
                            if(isJoinFeasible(label_forward, label_backward)) {
                                incumbent = cost;
                                joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                                break;
                            }
                        }
                        else break;
                    }
                }
                else break;
            }
        }
        else break;
    }
}

void LMBucket::ksol_orderedJoin(){
    auto objective = problem->getObj();
    std::multiset<std::tuple<cost_t, int, int>> orderedPairs;
    cost_t kth_cost = MAX_COST;
    cost_t cost;
    int i, j;

    populateQueues();
    
    // Sort queues by objective
    for(int p = 0; p < fw_queue.size(); p++){
        if(not fw_queue[p].empty())
            std::sort(
                fw_queue[p].begin(),
                fw_queue[p].end(),
                [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
        if(not bw_queue[p].empty())
            std::sort(
                bw_queue[p].begin(),
                bw_queue[p].end(),
                [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
    }

    //Prepare data structure
    for(i = 0; i < fw_queue.size(); i++)
        if(not fw_queue[i].empty() and i != problem->getDestination())
            for(j = 0; j < bw_queue.size(); j++)
                if(not bw_queue[j].empty() and j != problem->getOrigin() and problem->areNeighbors(i, j, true)) {
                    cost = objective->join(forward_best[i]->getObjective(), backward_best[j]->getObjective(), i, j);
                    orderedPairs.insert(std::make_tuple(cost, i, j));
                }

    //attempt join
    joinComparisons = 0;
    LabelAdv *label_forward, *label_backward;
    while(not orderedPairs.empty()){
        cost = std::get<0>(*orderedPairs.begin());
        if(cost > kth_cost)
            break;
        i = std::get<1>(*orderedPairs.begin());
        j = std::get<2>(*orderedPairs.begin());
        orderedPairs.erase(orderedPairs.begin());

        for(auto &label_forward : fw_queue[i]) {
            //If current label (i) + best label of j is worse than bound, skip to next i-j combination
            if(objective->join(label_forward->getObjective(), backward_best[j]->getObjective(), i, j) > kth_cost)
                break;

            for (auto &label_backward: bw_queue[j]){
                cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), i, j);
                if(cost > kth_cost)
                    break;

                joinComparisons++;
                if(isJoinFeasible(label_forward, label_backward)) {
                    if(cost < incumbent) incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                    if(joinable_labels.size() > requested_solutions) {
                        joinable_labels.erase(std::prev(joinable_labels.end()));
                        kth_cost = std::get<0>(*std::prev(joinable_labels.end()));
                    }
                }
            }
        }
    }
}

void LMBucket::ksol_orderedNodeJoin(){
    auto objective = problem->getObj();
    cost_t kth_cost = MAX_COST;
    joinComparisons = 0;

    populateQueues();

    for(int node = 0; node < problem->getNumNodes(); node++){
        auto & fw = fw_queue[node];
        auto & bw = bw_queue[node];

        if(fw.empty() or bw.empty())
            continue;

        cost_t best_cost = objective->join(forward_best[node]->getObjective(), backward_best[node]->getObjective(), node);
        if(best_cost > kth_cost)
            continue;

        std::sort(fw.begin(), fw.end(), [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
        std::sort(bw.begin(), bw.end(), [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });

        for(auto &label_forward : fw) {
            best_cost = objective->join(label_forward->getObjective(), backward_best[node]->getObjective(), node);
            //If current label + best backward is worse than bound, skip remaining forward labels
            if(best_cost > kth_cost)
                break;

            for (auto &label_backward : bw) {
                cost_t cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), node);
                if(cost > kth_cost)
                    break;

                joinComparisons++;
                if(isNodeJoinFeasible(label_forward, label_backward)) {
                    if(cost < incumbent) incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                    if(joinable_labels.size() > requested_solutions) {
                        joinable_labels.erase(std::prev(joinable_labels.end()));
                        kth_cost = std::get<0>(*std::prev(joinable_labels.end()));
                    }
                }
            }
        }
    }
}

void LMBucket::orderedNodeJoin() {
    auto objective = problem->getObj();
    joinComparisons = 0;

    populateQueues();
    
    for (int node = 0; node < problem->getNumNodes(); node++){
        auto & fw = fw_queue[node];
        auto & bw = bw_queue[node];

        if (fw.empty() or bw.empty())
            continue;

        cost_t best_cost = objective->join(forward_best[node]->getObjective(), backward_best[node]->getObjective(), node);
        
        if (best_cost > incumbent) 
            continue;

        std::sort(fw.begin(), fw.end(), [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
        std::sort(bw.begin(), bw.end(), [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });

        for(auto &label_forward : fw) {
            best_cost = objective->join(label_forward->getObjective(), backward_best[node]->getObjective(), node);
            if (best_cost > incumbent) 
                break;

            for (auto &label_backward : bw) {
                cost_t cost = objective->join(label_forward->getObjective(), label_backward->getObjective(), node);
                if (cost > incumbent) 
                    break;

                joinComparisons++;
                if(isNodeJoinFeasible(label_forward, label_backward)) {
                    incumbent = cost;
                    joinable_labels.insert(std::make_tuple(cost, label_forward, label_backward));
                    break;
                }
            }
        }
    }
}

void LMBucket::bucketNodeJoin() {

    auto objective = problem->getObj();
    joinComparisons = 0;
    cost_t cost;

    auto bucket_resource = problem->getRes(bucket_resource_id);

    for (int node = 0; node < problem->getNumNodes(); node++){
        fw_nonempty.clear();
        bw_nonempty.clear();
        
        for (auto bucket_idx = 0; bucket_idx < bucket_count; ++bucket_idx){
            if (not fw_bucket[bucket_idx][node].empty())
                fw_nonempty.push_back(bucket_idx);
            if (not bw_bucket[bucket_idx][node].empty())
                bw_nonempty.push_back(bucket_idx);
        }
        
        for (auto it_idx_fw = fw_nonempty.rbegin(); it_idx_fw != fw_nonempty.rend(); it_idx_fw++){
            auto idx_fw = *it_idx_fw;
            for (auto it_idx_bw = bw_nonempty.rbegin(); it_idx_bw != bw_nonempty.rend(); it_idx_bw++){
                auto idx_bw = *it_idx_bw;
                if (not bucket_resource->isFeasible(bucket_resource->join(
                    min_consumption_at_bucket[idx_fw], min_consumption_at_bucket[idx_bw], node)))
                    continue;

                for(auto &label_forward : fw_bucket[idx_fw][node]){
                    for(auto &label_backward : bw_bucket[idx_bw][node]){
                        cost = objective->join(label_forward.getObjective(), label_backward.getObjective(), node);
                        if (cost >= incumbent) 
                            break;
                        joinComparisons++;
                        if(isNodeJoinFeasible(&label_forward, &label_backward)){
                            incumbent = cost;
                            joinable_labels.insert(std::make_tuple(cost, &label_forward, &label_backward));
                            break;
                        }
                    } 
                }
            }
        }
    }
}

void LMBucket::ksol_bucketNodeJoin() {

    auto objective = problem->getObj();
    cost_t kth_cost = MAX_COST;
    joinComparisons = 0;

    auto bucket_resource = problem->getRes(bucket_resource_id);

    for (int node = 0; node < problem->getNumNodes(); node++){
        fw_nonempty.clear();
        bw_nonempty.clear();
        
        for (auto bucket_idx = 0; bucket_idx < bucket_count; ++bucket_idx){
            if (not fw_bucket[bucket_idx][node].empty())
                fw_nonempty.push_back(bucket_idx);
            if (not bw_bucket[bucket_idx][node].empty())
                bw_nonempty.push_back(bucket_idx);
        }
        
        for (auto it_idx_fw = fw_nonempty.rbegin(); it_idx_fw != fw_nonempty.rend(); it_idx_fw++){
            auto idx_fw = *it_idx_fw;
            for (auto it_idx_bw = bw_nonempty.rbegin(); it_idx_bw != bw_nonempty.rend(); it_idx_bw++){
                auto idx_bw = *it_idx_bw;
                if (not bucket_resource->isFeasible(bucket_resource->join(
                    min_consumption_at_bucket[idx_fw], min_consumption_at_bucket[idx_bw], node)))
                    continue;
                
                for(auto &label_forward : fw_bucket[idx_fw][node]){
                    for(auto &label_backward : bw_bucket[idx_bw][node]){
                        cost_t cost = objective->join(label_forward.getObjective(), label_backward.getObjective(), node);
                        if(cost > kth_cost)
                            break;

                        joinComparisons++;
                        if(isNodeJoinFeasible(&label_forward, &label_backward)){
                            if(cost < incumbent) incumbent = cost;
                            joinable_labels.insert(std::make_tuple(cost, &label_forward, &label_backward));
                            if(joinable_labels.size() > requested_solutions) {
                                joinable_labels.erase(std::prev(joinable_labels.end()));
                                kth_cost = std::get<0>(*std::prev(joinable_labels.end()));
                            }
                        }
                    } 
                }
            }
        }
    }
}

// Copy labels' pointers from buckets to node queues
void LMBucket::populateQueues(){
    for(int bucket_idx = 0; bucket_idx != bucket_count; ++bucket_idx)
        for(int node = 0; node != fw_bucket[bucket_idx].size(); ++node)
        {
            for(auto &f : fw_bucket[bucket_idx][node])
                fw_queue[node].push_back(&f);
            for(auto &b : bw_bucket[bucket_idx][node])
                bw_queue[node].push_back(&b);
        }
}

bool LMBucket::isJoinFeasible(LabelAdv* label_forward, LabelAdv* label_backward) {
    int i = label_forward->getNode();
    int j = label_backward->getNode();

    int snapshot_forward, snapshot_backward;
    std::vector<Resource*>& resources = problem->getResources();

    if (two_cycle_elimination){
        int i_predecessor = label_forward->getPredecessorNode();
        int j_predecessor = label_backward->getPredecessorNode();
        if (i == j_predecessor or j == i_predecessor)
            return false;
    }

    for(int resID = 0; resID < problem->getNumRes(); resID++) {
        snapshot_forward = label_forward->getSnapshot(resID);
        snapshot_backward = label_backward->getSnapshot(resID);
        int current_value = resources[resID]->join(snapshot_forward, snapshot_backward, i, j);
        if(not resources[resID]->isFeasible(current_value))
            return false;
    }

    return not intersects(label_forward->getVisited(), label_backward->getVisited());

}

bool LMBucket::isNodeJoinFeasible(LabelAdv* label_forward, LabelAdv* label_backward) {
    int node = label_forward->getNode();

    int snapshot_forward, snapshot_backward;
    std::vector<Resource*>& resources = problem->getResources();

    if (two_cycle_elimination) {
        int i_predecessor = label_forward->getPredecessorNode();
        int j_predecessor = label_backward->getPredecessorNode();
        if (j_predecessor == i_predecessor)
            return false;
    }

    for(int resID = 0; resID < problem->getNumRes(); resID++) {
        snapshot_forward = label_forward->getSnapshot(resID);
        snapshot_backward = label_backward->getSnapshot(resID);
        int current_value = resources[resID]->join(snapshot_forward, snapshot_backward, node);
        if(!resources[resID]->isFeasible(current_value))
            return false;
    }

    return not intersects_except(label_forward->getVisited(), label_backward->getVisited(), node);

}


/** Solution management +*/
//Return a solution
std::tuple<cost_t, LabelAdv*, LabelAdv*> LMBucket::getSolutionLabels() {
    std::tuple<cost_t, LabelAdv*, LabelAdv*> solution_data = {0, nullptr, nullptr};

    if(bidirectional and joinFound()) {
        solution_data = getBestJoin();
        cost_t best_objective = std::get<0>(solution_data);
        int best_rank = MAX_INT;

        //Among joins with the best objective, prefers elementary paths, then NG-routes
        for (auto & join : joinable_labels) {
            if (std::get<0>(join) != best_objective)
                break;

            int rank = getJoinRank(std::get<1>(join), std::get<2>(join));
            if (rank < best_rank) {
                best_rank = rank;
                solution_data = join;

                //An elementary path cannot be improved
                if (best_rank == 0)
                    break;
            }
        }
    }
    else {
        LabelAdv* candidate = getODLabel();
        if(candidate){
            std::get<0>(solution_data) = candidate->getObjective();
            candidate->getDirection()? std::get<1>(solution_data) = candidate : std::get<2>(solution_data) = candidate;
        }
    }

    return solution_data;
}

int LMBucket::getJoinRank(LabelAdv* label_forward, LabelAdv* label_backward){
    //Builds the tour of the joined path
    std::list<int> tour;
    bool node_join = label_forward->getNode() == label_backward->getNode();

    Label* label = label_forward;
    while(label != nullptr){
        tour.push_front(label->getNode());
        label = label->getPredecessor();
    }

    label = node_join ? label_backward->getPredecessor() : label_backward;
    while(label != nullptr){
        tour.push_back(label->getNode());
        label = label->getPredecessor();
    }

    //Checks elementarity and NG-route compliance
    std::vector<bool> visited(problem->getNumNodes(), false);
    Bitset memory(problem->getNumNodes());
    bool elementary = true;
    bool ng_route = unreachable_ng and not unreachable_ng->empty();

    for(int node: tour) {
        if(visited[node])
            elementary = false;
        if(ng_route and memory.get(node))
            ng_route = false;
        if(not elementary and not ng_route)
            return 2;

        visited[node] = true;
        if(ng_route) {
            memory &= (*unreachable_ng)[node];
            memory.set(node);
        }
    }

    //Rank 0: elementary path, rank 1: NG-route, rank 2: any other path
    if(elementary)
        return 0;
    return ng_route ? 1 : 2;
}

void LMBucket::sortLabelsInQueue(bool direction, int node) {
    auto& queue = direction ? fw_queue[node] : bw_queue[node];
    std::sort(queue.begin(), queue.end(), [&](LabelAdv* a, LabelAdv* b) { return a->getObjective() < b->getObjective(); });
}

const std::vector<LabelAdv*>& LMBucket::getLabelsInQueue(bool direction, int node){
    return direction ? fw_queue[node] : bw_queue[node];
}

/** Output **/
void LMBucket::printBucket(int bucket_idx, int node, bool direction){
    Logger::debug("Printing all labels of bucket " + std::to_string(bucket_idx) + " at node " + std::to_string(node) + " with direction " + std::to_string(direction));
    auto & bucket = direction ? fw_bucket[bucket_idx][node] : bw_bucket[bucket_idx][node];
    for (auto it = bucket.begin(); it != bucket.end(); ++it)
        it->printLabel();
}

/** Debug **/
//Finds labels with same obj and resource consumption
std::list<LabelAdv> LMBucket::buildTour(std::list<int> tour, bool direction) {
    if(not direction) tour.reverse();

    std::list<LabelAdv> tourLabels;

    tourLabels.emplace_back();
    LabelAdv* current_label = & tourLabels.front();

    int node = tour.front();
    tour.pop_front();

    current_label->initLabel(node, nullptr, direction, problem->getNumRes());
    current_label->setObjective(problem->getObj()->getInitValue() + problem->getObj()->getNodeCost(node));

    std::vector<Resource*>& resources = problem->getResources();
    for(int id = 0; id < problem->getNumRes(); id++)
        current_label->setSnapshot(id, resources[id]->getInitValue() + resources[id]->getNodeCost(node));

    for(auto node: tour) {
        LabelAdv new_label;
        extendLabel(current_label, &new_label, node);
        tourLabels.push_back(new_label);
        current_label = & tourLabels.back();
    }

    return tourLabels;
}

/** Data collection management **/

void LMBucket::initDataCollection() {
    if(not Parameters::isCollecting())
        return;

    collector.init("lm_name", name);
    collector.init("data_structure", lm_type);
    collector.init("executionID", 0);
    collector.init("compare_unreachables", 0);
    collector.init("candidate_type", "-1");
    collector.init("join_algo", "-1");
    collector.init("iterations", 0);
    collector.init("ins_attempts_fw", 0);
    collector.init("ins_attempts_bw", 0);
    collector.init("nlabels_inserted", 0);
    collector.init("nfw_inserted", 0);
    collector.init("nbw_inserted", 0);
    collector.init("ndominated", 0);
    collector.init("ndominated_fw", 0);
    collector.init("ndominated_bw", 0);
    collector.init("nclosed", 0);
    collector.init("nclosed_fw", 0);
    collector.init("nclosed_bw", 0);
    collector.init("njoin", "0");
    collector.init("split", split_ratio);
    collector.setHeader();

    initLabelDataCollection(&collector_label_fw);
    initLabelDataCollection(&collector_label_bw);
}
void LMBucket::initLabelDataCollection(DataCollector* collector_label) {
    if(Parameters::getCollectionLevel() < 4)
        return;

    //collector_label->init("executionID", 0);
    collector_label->init("iterations", 0);
    collector_label->init("direction", 0);
    collector_label->init("node", -1);
    collector_label->init("predecessor", -1);
    collector_label->init("objective", MAX_COST);
    collector_label->init("consumption_critical", MAX_INT);
    for(int i = 1; i < problem->getNumRes(); i++)
        collector_label->init("consumption_"+std::to_string(i), MAX_INT);
    collector_label->init("tour_length", -1);
    collector_label->init("nvisited", -1);
    collector_label->init("nunreachable", -1);
    collector_label->init("nvisited_unaltered", -1);
    collector_label->init("repeated_visits", -1);

    //Globally
    collector_label->init("nlabels_network", -1);
    collector_label->init("nlabels_dominated_network", -1);
    collector_label->init("nlabels_closed_network", -1);
    collector_label->init("nlabels_open_network", -1);
    collector_label->setHeader();
}

void LMBucket::collectData() {
    if(not Parameters::isCollecting())
        return;

    collector.collect("lm_name", name);
    collector.collect("executionID", executionID);
    collector.collect("iterations", iterations);
    collector.collect("compare_unreachables", compare_unreachables);
    std::string candidate_type_st;

    switch(candidate_type){
        case CANDIDATE_RR:  candidate_type_st = "round-robin"; break;
        default: candidate_type_st = "node"; break;
    }
    collector.collect("candidate_type", candidate_type_st);

    std::string join_algo_st;

    switch(join_algo){
        case JOIN_NAIVE:  join_algo_st = "naive (arc)"; break;
        case JOIN_CLASSIC:  join_algo_st = "classic (arc)"; break;
        case JOIN_ORDERED:  join_algo_st = "ordered (arc)"; break;
        case JOIN_PARETO_ARC:  join_algo_st = "pareto (arc)"; break;
        case JOIN_ORDERED_NODE:  join_algo_st = "ordered (node)"; break;
        case JOIN_KORDERED:  join_algo_st = "kordered (arc)"; break;
        case JOIN_KORDERED_NODE:  join_algo_st = "kordered (node)"; break;
        case JOIN_BUCKET_NODE:  join_algo_st = "bucket (node)"; break;
        case JOIN_KBUCKET_NODE:  join_algo_st = "kbucket (node)"; break;
        default: join_algo_st = "default"; break;
    }
    collector.collect("join_algo", join_algo_st);

    collector.collect("ins_attempts_fw", ins_attempts_fw);
    collector.collect("ins_attempts_bw", ins_attempts_bw);
    collector.collect("nlabels_inserted", ninserted_fw + ninserted_bw);
    collector.collect("nfw_inserted", ninserted_fw);
    collector.collect("nbw_inserted", ninserted_bw);
    collector.collect("ndominated", ndominated_fw + ndominated_bw);
    collector.collect("ndominated_fw", ndominated_fw);
    collector.collect("ndominated_bw", ndominated_bw);
    collector.collect("nclosed", nclosed_fw + nclosed_bw);
    collector.collect("nclosed_fw", nclosed_fw);
    collector.collect("nclosed_bw", nclosed_bw);
    collector.collect("njoin", std::to_string(joinComparisons));
    collector.saveRecord();
}

void LMBucket::collectLabel(DataCollector* collector_label, LabelAdv *l) {
    if(Parameters::getCollectionLevel() < 4)
        return;

    bool direction = l->getDirection();
    int node = l->getNode();

    //collector_label->collect("executionID", executionID);
    collector_label->collect("iterations", iterations);
    collector_label->collect("direction", l->getDirection());
    collector_label->collect("node", node);
    collector_label->collect("predecessor", l->getPredecessorNode());
    collector_label->collect("objective", l->getObjective());
    collector_label->collect("consumption_critical", l->getSnapshot(RES_CRITICAL));
    for(int i = 1; i < problem->getNumRes(); i++)
        collector_label->collect("consumption_"+std::to_string(i), l->getSnapshot(i));

    collector_label->collect("nvisited", (int) l->getVisited().count());
    collector_label->collect("nunreachable", (int) l->getUnreachable().count());

    //Globally
    int n_open = direction ? ninserted_fw - ndominated_fw - nclosed_fw : ninserted_bw  - ndominated_bw - nclosed_bw;
    collector_label->collect("nlabels_network", direction ? ninserted_fw : ninserted_bw);
    collector_label->collect("nlabels_dominated_network", direction? ndominated_fw : ndominated_bw);
    collector_label->collect("nlabels_closed_network", direction? nclosed_fw : nclosed_bw);
    collector_label->collect("nlabels_open_network", n_open);

    collector_label->saveRecord();
}


