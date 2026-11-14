#ifndef LABEL_H
#define LABEL_H
#include <iostream>
#include "utils/constants.h"
#include "utils/param.h"

class Label{

public:

    /** Label management **/
    //Constructors and destructors
    Label() {}
    Label(const Label &obj);

    ~Label() = default;

    //Init and updates
    void initLabel(int node, Label* predecessor, bool direction, int n_res);
    void updateLabel(int node, Label* predecessor);

    //Resources management
    cost_t getObjective(){return objective;}
    void setObjective(cost_t objective){this->objective = objective;}

    int getSnapshot(int resID) {return snapshot[resID];}
    std::vector<int> & getSnapshot() {return snapshot;}
    void setSnapshot(int resID, int value){snapshot[resID] = value;}
    void setSnapshot(std::vector<int> snapshot) {this->snapshot = snapshot;}

    /** Queries **/
    int getNode(){return node;}
    bool getDirection(){return direction;}
    Label* getPredecessor(){return predecessor;}
    int getPredecessorNode(){return predecessor ? predecessor->getNode() : -1;}

    bool isObjectiveSet(){return objective != MAX_COST;}

    /** Operators **/
    void operator= (const Label& obj);
    bool operator== (const Label& obj);
    bool operator!= (const Label& obj);

    /** Output management **/
    void printLabel();
    void printPredecessors();

    /** Comparators **/
    static bool cmpObj(Label* a, Label* b) {return a->getObjective() < b->getObjective();}

protected:
    int node = -1;
    bool direction = true;
    Label* predecessor = nullptr;

    //Resources
    cost_t objective = MAX_COST;
    std::vector<int> snapshot;
};

#endif