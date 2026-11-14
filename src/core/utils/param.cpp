#include <iostream>
#include <sstream>
#include <math.h>
#include <functional>
#include <filesystem>
#include "param.h"
#include "utils/logger.h"

/**Default Values**/
std::string Parameters::param_path = "pathwyse.set";

/**Solver Parameters**/
std::string Parameters::instance_path = "input.txt";
int Parameters::verbosity = 2;

//Algorithm selection parameters
std::vector<std::string> Parameters::algo_names = {"PWDefault"};
std::vector<bool> Parameters::algo_active = {true};
bool Parameters::algo_parallel = false;

/**Problem parameters**/
int Parameters::memory_threshold = 10000;

//Override Problem Data
int Parameters::origin = -1;
int Parameters::destination = -1;
std::vector<int> Parameters::ub_resources;

//Decimal digits
int Parameters::decimal_digits = 2;

//Coordinates
float Parameters::coord_scaling = 1.0;
float Parameters::coord_distance_scaling = 0.95;

/**Preprocessing**/
int Parameters::preprocessing_intensity = PREPROCESSING_STANDARD;
bool Parameters::preprocessing_critical = false;

/**Default Algorithm (PWDefault)**/
int Parameters::requested_solutions = 1;
bool Parameters::default_autoconfig = true;
float Parameters::default_timelimit = 0;
bool Parameters::default_parallel = false;
int Parameters::default_search = SEARCH_BIDIRECTIONAL;
double Parameters::default_split = 0.5;
int Parameters::default_reserve = 10000000;

bool Parameters::default_use_visited = true;
bool Parameters::default_compare_unreachables = true;
int Parameters::default_dssr = DSSR_STANDARD;
int Parameters::default_ng = NG_OFF;
int Parameters::default_ng_size = 8;
std::string Parameters::default_ng_set_path = "";

int Parameters::default_candidate_type = CANDIDATE_NODE;
int Parameters::default_join_type = JOIN_ORDERED;
bool Parameters::default_earlyjoin = false;
unsigned long long int Parameters::default_earlyjoin_step = 300;

int Parameters::default_queue_limit = 20;

bool Parameters::two_cycle_elimination = false;

/**Bucket algorithm (PWBucket)**/
int Parameters::bucket_resource = 0;
std::string Parameters::bucket_size_mode = "unit_size";
float Parameters::bucket_prev_percentage = 1.0;
float Parameters::bucket_next_percentage = 1.0;

/**Data Collection**/
bool Parameters::output_write = false;
std::string Parameters::timestamp = "YYYY-MM-DD_hh:mm:ss";
int Parameters::collection_level = -1;
std::string Parameters::collection_folder = "output/";
std::string Parameters::collection_tag;
std::string Parameters::collection_path;

void Parameters::readParameters(std::string param_path) {
    std::ifstream f;
    std::string command, value;
    char separator;

    f.open(param_path.empty() ? Parameters::param_path : param_path);
    if(not f) {
        Logger::warn("No param file found!");
        return;
    }

    std::unordered_map<std::string, std::function<void(const std::string&)>> command_map = {
        {"verbosity", [&](const std::string& val) { verbosity = std::stoi(val); }},
        {"algo_names", [&](const std::string& val) {
            algo_names.clear();
            std::string name;
            std::stringstream ss(val);
            while(std::getline(ss, name, ','))
                algo_names.emplace_back(name);
        }},
        {"algo_active", [&](const std::string& val) {
            algo_active.clear();
            std::string token;
            std::stringstream ss(val);
            while(std::getline(ss, token, ','))
                algo_active.push_back(std::stoi(token));
        }},
        {"algo_parallel", [&](const std::string& val) { algo_parallel = std::stoi(val); }},
        {"problem/coordinates/distance/scaling", [&](const std::string& val) { coord_distance_scaling = std::stof(val); }},
        {"problem/coordinates/microdegrees", [&](const std::string& val) { if(std::stof(val) > 0) coord_scaling = 1/1e6; }},
        {"problem/memory_threshold", [&](const std::string& val) { memory_threshold = std::stoi(val); }},
        {"problem/decimal_digits", [&](const std::string& val) { decimal_digits = std::stoi(val); }},
        {"algo/preprocessing/intensity", [&](const std::string& val) {
            if(val == "full") preprocessing_intensity = PREPROCESSING_FULL;
            else if(val == "standard") preprocessing_intensity = PREPROCESSING_STANDARD;
            else preprocessing_intensity = PREPROCESSING_OFF;
        }},
        {"algo/preprocessing/critical", [&](const std::string& val) { preprocessing_critical = std::stoi(val); }},
        {"algo/default/autoconfig", [&](const std::string& val) { default_autoconfig = std::stoi(val); }},
        {"algo/default/timelimit", [&](const std::string& val) { default_timelimit = std::stof(val); }},
        {"algo/default/parallel", [&](const std::string& val) { default_parallel = std::stoi(val); }},
        {"algo/default/search", [&](const std::string& val)
        {
            if (val == "forward") default_search = SEARCH_FORWARD;
            else if (val == "backward") default_search = SEARCH_BACKWARD;
            else default_search = SEARCH_BIDIRECTIONAL;
        }},
        {"algo/default/bidirectional/split", [&](const std::string& val) { default_split = std::stof(val); }},
        {"algo/default/reserve", [&](const std::string& val) { default_reserve = std::stoi(val); }},
        {"algo/default/use_visited", [&](const std::string& val) { default_use_visited = std::stoi(val); }},
        {"algo/default/compare_unreachables", [&](const std::string& val) { default_compare_unreachables = std::stoi(val); }},
        {"algo/default/dssr", [&](const std::string& val) {
            if(val == "off") default_dssr = DSSR_OFF;
            else if(val == "restricted") default_dssr = DSSR_RESTRICTED;
            else default_dssr = DSSR_STANDARD;
        }},
        {"algo/default/ng", [&](const std::string& val) {
            if(val == "off") default_ng = NG_OFF;
            else if(val == "restricted") default_ng = NG_RESTRICTED;
            else default_ng = NG_STANDARD;
        }},
        {"algo/default/ng/set_size", [&](const std::string& val) { default_ng_size = std::stoi(val); }},
        {"algo/default/ng/set_path", [&](const std::string& val) { default_ng_set_path = val; }},
        {"algo/default/candidate/type", [&](const std::string& val) {
            if(val == "round-robin") default_candidate_type = CANDIDATE_RR;
            else default_candidate_type = CANDIDATE_NODE;
        }},
        {"algo/default/join/type", [&](const std::string& val) {
            if(val == "classic") default_join_type = JOIN_CLASSIC;
            else if(val == "classic_node") default_join_type = JOIN_CLASSIC_NODE;
            else if(val == "pareto") default_join_type = JOIN_PARETO_ARC;
            else if(val == "pareto_node") default_join_type = JOIN_PARETO_NODE;
            else if(val == "naive") default_join_type = JOIN_NAIVE;
            else if(val == "naive_node") default_join_type = JOIN_NAIVE_NODE;
            else if (val == "ordered") default_join_type = JOIN_ORDERED;
            else if (val == "ordered_node") default_join_type = JOIN_ORDERED_NODE;
            else if (val == "kordered") default_join_type = JOIN_KORDERED;
            else if (val == "kordered_node") default_join_type = JOIN_KORDERED_NODE;
            else if (val == "bucket_node") default_join_type = JOIN_BUCKET_NODE;
            else if (val == "kbucket_node") default_join_type = JOIN_KBUCKET_NODE;
            else default_join_type = JOIN_CLASSIC;
        }},
        {"algo/requested_solutions", [&](const std::string& val) { requested_solutions = std::stoi(val);}},
        {"algo/default/join/early", [&](const std::string& val) { default_earlyjoin = std::stoi(val); }},
        {"algo/default/join/early/step", [&](const std::string& val) { default_earlyjoin_step = std::stoi(val); }},
        {"algo/default/relaxations/queue_limit", [&](const std::string& val) { default_queue_limit = std::stoi(val); }},
        {"algo/two_cycle_elimination", [&](const std::string& val){two_cycle_elimination = std::stoi(val);}},
        {"algo/bucket/resource", [&](const std::string& val) { bucket_resource = std::stoi(val); }},
        {"algo/bucket/size_mode", [&](const std::string& val) { 
            if(val == "min_res_consumption" or val == "unit_size") 
                bucket_size_mode = val;
        }},
        {"algo/bucket/prev_dom_check_percentage", [&](const std::string& val) { bucket_prev_percentage = std::stof(val); }},
        {"algo/bucket/next_dom_check_percentage", [&](const std::string& val) { bucket_next_percentage = std::stof(val); }},
        {"data_collection/level", [&](const std::string& val) { collection_level = std::stoi(val); }},
        {"data_collection/tag", [&](const std::string& val) { if(collection_tag.empty()) collection_tag = val; }},
        {"output/write", [&](const std::string& val) { output_write = std::stoi(val); }}
    };

    // Helper
    auto removeLeadingAndTrailingSpaces = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r");
        if (start == std::string::npos) return std::string("");
        size_t end = s.find_last_not_of(" \t\r");
        return s.substr(start, end - start + 1);
    };

    //Loop over lines
    std::string line;
    while (getline(f, line)) {

        auto hash_pos = line.find('#');
        if (hash_pos != std::string::npos)
            line.erase(hash_pos);
        
        auto equal_pos = line.find('=');
        if (equal_pos != std::string::npos)
        {
            command = removeLeadingAndTrailingSpaces(line.substr(0, equal_pos));
            value   = removeLeadingAndTrailingSpaces(line.substr(equal_pos + 1));
            if(auto it = command_map.find(command); it != command_map.end())
                it->second(value);
        }
    }

    f.close();

    if(not isCollecting())
        output_write = false;
}

//Output path setup
void Parameters::setupCollectionPath(){
    collection_path = collection_folder + "/" + timestamp;
    collection_path += (not collection_tag.empty() ? "[" + collection_tag + "]" : "");

    int name_count = 1;
    std::string tmp_path = collection_path;

    while(std::filesystem::exists(tmp_path)){
        tmp_path = collection_path + "_" + std::to_string(name_count);
        name_count++;
    }

    collection_path = tmp_path + "/";
}


// Parse console elements for console execution.
// Returns True if successful, False otherwise.
bool Parameters::parseConsole(int argc, char **argv) {

    if(argc <= 1) {
        Logger::error("Instance file is missing...terminating.");
        return false;
    }

    // Mapping of command-line options to functions
    std::unordered_map<std::string, std::function<void(const std::string&)>> command_map = {
        {"-o", [&](const std::string& value) { origin = std::stoi(value); }},
        {"-d", [&](const std::string& value) { destination = std::stoi(value); }},
        {"-ub", [&](const std::string& value) { ub_resources.push_back(std::stoi(value)); }},
        {"-param", [&](const std::string& value) { param_path = value; }},
        {"-tag", [&](const std::string& value) { collection_tag = value; }}
    };

    int pos = 1;
    instance_path = argv[pos++];

    std::string current_command;
    while(pos < argc) {
        std::string arg = argv[pos++];

        if(arg[0] == '-' and !isdigit(arg[1])) {
            current_command = arg;
            if(pos == argc) {
                std::cout << "Missing value for parameter: " << current_command << std::endl;
                return false;
            }
            continue;
        }

        auto it = command_map.find(current_command);

        if(it == command_map.end()) {
            std::cout << "Unrecognized parameter: " << current_command << std::endl;
            return false;

        }
        it->second(arg);
    }

    return true;
}