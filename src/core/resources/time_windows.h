#ifndef TIME_WINDOWS_H
#define TIME_WINDOWS_H

#include "resource.h"

class TimeWindow: public Resource {

public:
    TimeWindow();
    ~TimeWindow() = default;

    void init(int origin, int destination) override;
    int extend(int current_value, int i, int j, bool direction) override;
    int join(int current_value_forward, int current_value_backward, int i, int j) override;
    int join(int current_value_forward, int current_value_backward, int node) override;
    bool isFeasible(int current_value, int current_node, double bounding, bool direction) override;
    int getMinConsumption(int i, int j, bool direction) override;
};

#endif
