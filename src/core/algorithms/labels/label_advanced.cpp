#include "label_advanced.h"

/** Label management **/
//Constructors and destructors
LabelAdv::LabelAdv(const LabelAdv &obj) : Label(obj) {
    label_type = obj.label_type;
    unreachable_count = obj.unreachable_count;
    extension_target = obj.extension_target;
    visited = obj.visited;
    unreachable = obj.unreachable;
}

//Init and updates
void LabelAdv::initVisited(int node, int n_nodes){
    visited = Bitset(n_nodes);
    visited.set(node);
    unreachable = visited;
    unreachable_count = 1;
}

void LabelAdv::updateLabel(int node, LabelAdv* predecessor) {
    Label::updateLabel(node, predecessor);
    extension_target = ALL;
    label_type = UNDEFINED;
    if(not visited.empty()) {
        visited.set(node);
        setUnreachable(node);
    }
}

/** Operators **/
void LabelAdv::operator= (const LabelAdv& obj)  {
    Label::operator=(obj);

    extension_target = obj.extension_target;
    label_type = obj.label_type;
    unreachable_count = obj.unreachable_count;
    visited = obj.visited;
    unreachable = obj.unreachable;
}

bool LabelAdv::operator== (LabelAdv& obj)  {
    if(visited != obj.visited or unreachable != obj.unreachable)
        return false;

    return Label::operator==(obj);
}

bool LabelAdv::operator!= (LabelAdv& obj)  {
    if(visited != obj.visited or unreachable != obj.unreachable)
        return true;

    return Label::operator!=(obj);
}
