#include <limits>
#include <cstdint>

/** Objective cost type **/
//For smaller costs, change to int32_t (and MAX_COST_DIGITS below to 9), then rebuild everything again.
using cost_t = int64_t;
#define MAX_COST_DIGITS 12

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#define NO_INLINE __declspec(noinline)
#else
#define FORCE_INLINE [[gnu::always_inline]] inline
#define NO_INLINE [[gnu::noinline]]
#endif

/** Status **/

//Problem status
#define PROBLEM_INDETERMINATE 0
#define PROBLEM_FEASIBLE 1
#define PROBLEM_INFEASIBLE 2

//Solver status
#define SOLVER_START 0
#define SOLVER_READY 1
#define SOLVER_BUSY 2

//Algorithm status
#define ALGO_READY 0
#define ALGO_OPTIMIZING 1
#define ALGO_DONE 2
#define ALGO_BOUNDLIMIT 3
#define ALGO_GAPLIMIT 4
#define ALGO_TIMELIMIT 5

//Solution status
#define PATH_UNKNOWN 0
#define PATH_SUPEROPTIMAL 1
#define PATH_OPTIMAL 2
#define PATH_FEASIBLE 3
#define PATH_INFEASIBLE 4
#define PATH_NG 5

//Search
#define SEARCH_BIDIRECTIONAL (-1)
#define SEARCH_FORWARD 0
#define SEARCH_BACKWARD 1


/** Solution **/

//Solution ranking selection
#define RANK_OBJECTIVE ("Objective")

//Solution values
#define MAX_INT std::numeric_limits<int>::max()
#define MAX_DOUBLE std::numeric_limits<double>::infinity()
#define MAX_COST std::numeric_limits<cost_t>::max()
#define MIN_INT std::numeric_limits<int>::min()
#define MIN_DOUBLE (-std::numeric_limits<double>::infinity())
#define MIN_COST std::numeric_limits<cost_t>::min()

/** Problem **/

//Resource type creation
#define RES_CAPACITY 0
#define RES_TIME 1
#define RES_TIMEWINDOWS 2
#define RES_NODELIM 3

//Resource selection
#define RES_COST (-1)
#define RES_CRITICAL 0

//Distance type
#define DIST_NONE 0
#define DIST_2D 1
#define DIST_GEOGRAPHIC 2

//Distance algorithm
#define DIST_ALGO_NONE 0
#define DIST_ALGO_EUCLIDEAN 1
#define DIST_ALGO_HAVERSINE 2
#define DIST_ALGO_EQUIRECTANGULAR 3

//Numerical values
#define MAX_JOIN_STEP 4294967296
#define EARTH_RADIUS 6371008.8

/** Algorithm **/

//Algorithm selection
#define MAIN_ALGORITHM (-1)

//Algorithm type
#define ALGO_EXACT ("Exact")
#define ALGO_HEURISTIC ("Heuristic")
#define ALGO_RELAXATION ("Relaxation")

//Preprocessing type
#define PREPROCESSING_OFF (-1)
#define PREPROCESSING_STANDARD 0
#define PREPROCESSING_FULL 1

//Bidirectional algorithm direction
#define FORWARD 1
#define BACKWARD 0

//Relaxation type
#define RELAX_GLOBAL 0 //DSSR
#define RELAX_LOCAL 1 //All the others

//Relaxation mode selection
#define DSSR_OFF 0
#define DSSR_STANDARD 1
#define DSSR_RESTRICTED 2
#define NG_OFF 0
#define NG_STANDARD 1
#define NG_RESTRICTED 2

//Extension: Node Targets
#define ALL (-1)

//Label type (for 2-cycle elimination)
//Default type
#define UNDEFINED (-1)
//Labels that cannot go back to the previous node
#define DOMINANT 0
//Labels that can go back to the previous node
#define PARTIALLY_DOMINANT 1
//Dominated labels (cannot be discarded)
#define DOMINATED 2

//Candidate Types
#define CANDIDATE_RR 0
#define CANDIDATE_NODE 1

//Join type
#define JOIN_ARC 0
#define JOIN_NODE 1

//Join Algo
#define JOIN_NAIVE 0
#define JOIN_NAIVE_NODE 1
#define JOIN_CLASSIC 2
#define JOIN_CLASSIC_NODE 3
#define JOIN_ORDERED 4
#define JOIN_ORDERED_NODE 5
#define JOIN_PARETO_ARC 6
#define JOIN_PARETO_NODE 7
#define JOIN_KORDERED 8
#define JOIN_KORDERED_NODE 9
#define JOIN_BUCKET_NODE 10
#define JOIN_KBUCKET_NODE 11

//Insertion
#define INSERT_OBJ 0
#define INSERT_BITSET 1
#define INSERT_UNORDERED 2

//Extension
#define FULL 0
#define PROMISING 1
#define COMPLEMENT 2

/** Data collection **/

//Data type selection
#define TEXT 0
#define INTEGER 1
#define DECIMALS 2
#define TIME 3

//Data collection index
#define T_INS_FW 4
#define T_INS_BW 5

//Verbosity levels
#define VERB_SILENT 0
#define VERB_STD 1
#define VERB_DETAIL 2
#define VERB_DATA 3
#define VERB_DEBUG 4

/** Interface **/
#define RETURN_ERROR 0
#define RETURN_OK 1

/** Utilities **/
#define EPS 10E-7

/** Text **/
#define COL_RESET  "\033[0m"
#define BOLD   "\033[1m"
#define DIM    "\033[2m"
#define CYAN   "\033[36m"
#define YELLOW "\033[33m"
#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define GRAY   "\033[90m"
