#ifndef DEFAULTCOST_H
#define DEFAULTCOST_H

#include "resource.h"

class DefaultCost: public CostResource {

public:
    DefaultCost();
    ~DefaultCost() = default;

    cost_t extend(cost_t current_value, int i, int j, bool direction) override;
    cost_t join(cost_t current_value_forward, cost_t current_value_backward, int i, int j) override;
    cost_t join(cost_t current_value_forward, cost_t current_value_backward, int node) override;
    bool isFeasible(cost_t current_value, int current_node, double bounding, bool direction) override;
};


#endif
