#ifndef LABEL_ADVANCED_H
#define LABEL_ADVANCED_H

#include "label.h"
#include "utils/bitset.h"

class LabelAdv: public Label {

public:

    /** Label management **/
    //Constructors and destructors
    LabelAdv() {};
    LabelAdv(const LabelAdv &obj);

    ~LabelAdv() = default;

    //Init and updates
    void initVisited(int node, int n_nodes);
    void updateLabel(int node, LabelAdv* predecessor);

    //Visited and unreachable management
    bool isReachable(int node) {return unreachable.get(node) == 0;}
    void updateUnreachables(Bitset & mask) {unreachable &= mask; visited &= mask; refreshUnreachableCount();}
    Bitset & getVisited(){return visited;}
    bool getVisitedActive(int i){return visited.get(i);}
    Bitset & getUnreachable(){return unreachable;}
    bool getUnreachableActive(int i){return unreachable.get(i);}


    void setUnreachable(int node){if(not unreachable.get(node)) unreachable_count++; unreachable.set(node);}
    int getUnreachableCount(){return unreachable_count;}
    void refreshUnreachableCount(){unreachable_count = (uint16_t) unreachable.count();}

    //Dominated and Elementary status management
    bool isDominated(){return label_type == DOMINATED;}
    void setDominated(){this->label_type = DOMINATED;}

    //Extension target and label type management
    int getExtensionTarget(){return extension_target;}
    void setExtensionTarget(int extension_target){this->extension_target = extension_target;}
    short getLabelType(){return label_type;}
    void setLabelType(short label_type){this->label_type = label_type;}

    /** Operators **/
    void operator= (const LabelAdv& obj);
    bool operator== (LabelAdv& obj);
    bool operator!= (LabelAdv& obj);

    /** Output management **/
    void printVisited() {std::cout<<visited<<std::endl;}
    void printUnreachable() {std::cout<<unreachable<<std::endl;}

protected:
    short label_type = UNDEFINED;
    uint16_t unreachable_count = 0;
    int extension_target = ALL;
    Bitset visited;
    Bitset unreachable;
};


#endif