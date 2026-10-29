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

    ~LabelAdv() override = default;

    //Init and updates
    void initVisited(int node, int n_nodes);
    void updateLabel(int node, LabelAdv* predecessor);

    //Visited and unreachable management
    bool isReachable(int node) {return unreachable.get(node) == 0;}
    void updateUnreachables(Bitset & mask) {unreachable &= mask; visited &= mask;}
    Bitset & getVisited(){return visited;}
    bool getVisitedActive(int i){return visited.get(i);}
    Bitset & getUnreachable(){return unreachable;}
    bool getUnreachableActive(int i){return unreachable.get(i);}


    //Open pickups (picked up, not yet delivered); allocated on first use
    void ensureOpen(int n_nodes) {if(open.empty()) open = Bitset(n_nodes);}
    bool isOpen(int node) {return not open.empty() and open.get(node);}
    bool hasOpen() const {return open.any();}
    void setOpen(int node) {open.set(node);}
    void clearOpen(int node) {if(not open.empty()) open.reset(node);}
    Bitset & getOpen(){return open;}

    void setUnreachable(int node){unreachable.set(node);}
    int getUnreachableCount(){return unreachable.count();}

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
    int extension_target = ALL;
    Bitset visited;
    Bitset unreachable;
    Bitset open;
};


#endif