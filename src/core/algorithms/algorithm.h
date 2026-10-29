#ifndef ALGO_H
#define ALGO_H
#include "data/problem.h"
#include "data/path.h"
#include "utils/data_collector.h"
#include "utils/constants.h"

class Algorithm {

public:

    /** Algorithm management **/
    //Constructors and destructors
    explicit Algorithm(std::string name, Problem* problem);
    virtual ~Algorithm() = default;

    void initAlgorithm();
    virtual void solve() = 0;
    virtual void resetAlgorithm(int reset_level) = 0;

    //Data and queries
    void setName(std::string name) {this->name = name;}
    std::string getName() {return name;}

    int getStatus() {return algo_status;}
    void setStatus(int status){this->algo_status = status;}

    void setExecutionID(int id) {executionID = id;}
    int getExecutionID() {return executionID;}

    void setType(std::string type) {algo_type = type;}
    std::string getType() {return algo_type;}

    //Problem data management
    Problem* getProblem() {return problem;}
    void setProblem(Problem* problem) {this->problem = problem;}

    /** Solution management **/
    //Solution process
    bool updateIncumbent(int value);
    void setIncumbent(int value){incumbent = value;}
    int getIncumbent(){return incumbent;}

    bool updateLowerBound(int value);
    void setLowerBound(int value){lower_bound = value;}
    int getLowerBound(){return lower_bound;}

    double getGlobalTime(){return collector.getGlobalTime();}

    //Per-solve time limit in seconds (overrides the settings file, kept over resets); 0 = use the settings file
    void setTimelimit(double seconds) {timelimit_override = seconds > 0 ? (float)seconds : 0.0f; if(timelimit_override > 0) timelimit = timelimit_override;}

    //Statistics of the last solve (labeling algorithms)
    virtual unsigned long long getLabelCount() {return 0;}
    virtual int getRounds() {return 0;}


    //Solution
    Path* getBestSolution() {return best_solution_id >= 0 ? &solutions[best_solution_id] : nullptr;}
    bool isBestSolutionElementary() {return getBestSolution() and getBestSolution()->isElementary();}
    void updateBestSolution(int id){best_solution_id = id;}
    std::vector<Path> & getSolutions() {return solutions;}
    void addSolution(Path & path);
    void buildPath(int objective, Label* forward_label, Label* backward_label = nullptr);
    void clearSolutions() {solutions.clear(); best_solution_id = -1;}
    /** Output management **/
    void printStatus();
    void printGlobalTime();
    void printAlgorithm();
    void printCollection() {collector.print();}
    /** Timelimit management **/
    bool isTimeLimitReached();

    /** Data collection management **/
    //Overridable methods for custom initialization and data collection
    void initDataCollection();
    void collectSolution(int id);

protected:

    //Problem
    Problem* problem;

    //Algorithm
    std::string name;
    std::string algo_type;

    //Configuration
    bool parallel;
    bool bidirectional;

    //Optimization run information
    int executionID;
    int iterations;
    int algo_status;
    int incumbent;
    int lower_bound;
    float timelimit;
    float timelimit_override = 0.0f;
    bool timeout;

    //Solution
    int best_solution_id;
    std::vector<Path> solutions;

    //Data collection
    DataCollector collector;        //Data collector for algorithm information
    DataCollector collector_sol;    //Data collector for solution information
};

#endif