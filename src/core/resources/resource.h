#ifndef RESOURCE_H
#define RESOURCE_H

#include <iostream>
#include <limits>
#include "data/resource_data.h"
#include "utils/constants.h"

//T is the value type: cost_t for the objective, int for resource consumptions.
template<typename T>
class ResourceT {

public:

    /** Resource management **/
    //Constructors and destructors
    ResourceT() {
        init_value = 0;
        lower_bound = std::numeric_limits<T>::min();
        upper_bound = std::numeric_limits<T>::max();
        bucket_compatible = true;
        lower_bound_d = MIN_DOUBLE;
        upper_bound_d = MAX_DOUBLE;
    };

    virtual ~ResourceT() {delete data;}

    //General info
    void setName(std::string name) {this->name = name;}
    std::string getName(){return name;}
    bool isBucketCompatible(){return bucket_compatible;}

    //Starting value
    T getInitValue(){return init_value;}

    /** Bounds **/
    //Global bounds
    void setBounds(T lb, T ub){lower_bound = lb; upper_bound = ub;}
    void setBoundsDouble(double lb, double ub){lower_bound_d = lb; upper_bound_d = ub;}
    void setLB(T lower_bound){this->lower_bound = lower_bound;}
    void setUB(T upper_bound){this->upper_bound = upper_bound;}
    void setLBDouble(double v){lower_bound_d = v;}
    void setUBDouble(double v){upper_bound_d = v;}
    T getLB(){return lower_bound;}
    T getUB(){return upper_bound;}
    double getLBDouble(){return lower_bound_d;}
    double getUBDouble(){return upper_bound_d;}

    //Node bounds
    void setNodeBounds(std::vector<T> node_lower_bound, std::vector<T> node_upper_bound) {
        this->node_lower_bound = node_lower_bound;
        this->node_upper_bound = node_upper_bound;
    }

    void setNodeBound(int n_nodes, int i, T node_lower_bound, T node_upper_bound){
        if(this->node_lower_bound.empty())
            this->node_lower_bound.resize(n_nodes, 0);
        if(this->node_upper_bound.empty())
            this->node_upper_bound.resize(n_nodes, 0);

        this->node_lower_bound[i] = node_lower_bound;
        this->node_upper_bound[i] = node_upper_bound;
    }

    T getNodeLB(int node){return node_lower_bound[node];}
    T getNodeUB(int node){return node_upper_bound[node];}
    std::vector<T> & getNodesLB(){return node_lower_bound;}
    std::vector<T> & getNodesUB(){return node_upper_bound;}

    /** Double representation **/
    void setArcCostDouble(int i, int j, double cost) { data->setArcCostDouble(i, j, cost); }
    double getArcCostDouble(int i, int j) { return data->getArcCostDouble(i, j); }
    void setNodeCostDouble(int id, double cost) { data->setNodeCostDouble(id, cost); }
    double getNodeCostDouble(int id) { return data->getNodeCostDouble(id); }
    std::vector<double> getNodeCostsDouble() { return data->getNodeCostsDouble(); }
    double getMaxAbsValue() {
        double max = data->getMaxAbsValue();
        if (std::isfinite(lower_bound_d)) max = std::max(max, std::abs(lower_bound_d));
        if (std::isfinite(upper_bound_d)) max = std::max(max, std::abs(upper_bound_d));
        return max;
    }
    void applyScale(double scale) {
        data->applyScale(scale);
        if (std::isfinite(lower_bound_d)) lower_bound = (T)(lower_bound_d * scale);
        if (std::isfinite(upper_bound_d)) upper_bound = (T)(upper_bound_d * scale);
    }

    /** Resource data structure management **/
    void initData(bool compress_data = false, int n_nodes = 1) {compress_data ? data = new ResourceDataMapT<T>(n_nodes): data = new ResourceDataMatrixT<T>(n_nodes);}
    void setData(ResourceDataT<T>* data) {this->data = data;}
    ResourceDataT<T>* getData() {return data;}

    T getArcCost(int i, int j) {return data->getArcCost(i,j);}
    void setArcCost(int i, int j, T cost) {data->setArcCost(i,j, cost);}

    T getNodeCost(int i) {return data->getNodeCost(i);}
    std::vector<T> getNodeCosts() {return data->getNodeCosts();}
    void setNodeCost(int i, T cost) { data->setNodeCost(i, cost);}
    void setNodeCosts(std::vector<T> costs) { data->setNodeCosts(costs);}

    bool hasNegativeCost(){return data->hasNegativeValue();}

    /** Resource behaviour definition **/

    virtual void init(int origin, int destination) {}
    virtual T extend(T current_value, int i, int j, bool direction = true) = 0;
    virtual T join(T current_value_forward, T current_value_backward, int i, int j) = 0;
    virtual T join(T current_value_forward, T current_value_backward, int node) = 0;
    virtual bool isFeasible(T current_value, int current_node = -1, double bounding = 1, bool direction = true) = 0;
    virtual T getMinConsumption(int i, int j, bool direction = true) {return extend(0, i, j, direction);}

protected:

    std::string name;

    T init_value;
    bool bucket_compatible;
    T lower_bound, upper_bound;
    double lower_bound_d, upper_bound_d;
    std::vector<T> node_lower_bound, node_upper_bound;

    ResourceDataT<T>* data;
};

using Resource = ResourceT<int>;
using CostResource = ResourceT<cost_t>;

#endif
