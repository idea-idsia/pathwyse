#include "defaultcost.h"

DefaultCost::DefaultCost() {
    name = "DefaultCost";
    init_value = 0;
}

cost_t DefaultCost::extend(cost_t current_value, int i, int j, bool direction) {
    int dest_node = direction ? j : i;
    return current_value + data->getArcCost(i, j) + data->getNodeCost(dest_node);
}

cost_t DefaultCost::join(cost_t current_value_forward, cost_t current_value_backward, int i, int j){
    return current_value_forward + data->getArcCost(i, j) + current_value_backward;
}

cost_t DefaultCost::join(cost_t current_value_forward, cost_t current_value_backward, int node){
    return current_value_forward + current_value_backward - data->getNodeCost(node);
}

bool DefaultCost::isFeasible(cost_t current_value, int current_node, double bounding, bool direction) {
    return current_value <= upper_bound;
}
