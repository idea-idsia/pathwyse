#Minimum CMake version
cmake_minimum_required(VERSION 3.20)

#Compilation flags
set(CMAKE_CXX_STANDARD 20)

if (MSVC)
	set(CMAKE_CXX_FLAGS  "/O2 /w  /DNDEBUG")
elseif (WIN32)
	# MinGW-w64 GCC on Windows
	set(CMAKE_CXX_FLAGS  "-O3 -w -DNDEBUG")
else()
	set(CMAKE_CXX_FLAGS  "-O3 -w -pthread -fopenmp -DNDEBUG")
endif()

#Project information
project(pathwyse)

set(
        SOURCE_FILES_LIB
        core/solver.h
        core/solver.cpp

        core/data/graph.h
        core/data/graph.cpp
        core/data/path.h
        core/data/path.cpp
        core/data/problem.h
        core/data/problem.cpp
        core/data/resource_data.h

        core/resources/resource.h
        core/resources/defaultcost.h
        core/resources/defaultcost.cpp
        core/resources/capacity.h
        core/resources/capacity.cpp
        core/resources/node_limit.h
        core/resources/node_limit.cpp
        core/resources/time.h
        core/resources/time.cpp
        core/resources/time_windows.h
        core/resources/time_windows.cpp
	
        core/algorithms/algorithm.h
        core/algorithms/algorithm.cpp

        core/algorithms/labels/label.h
        core/algorithms/labels/label.cpp
        core/algorithms/labels/label_advanced.h
        core/algorithms/labels/label_advanced.cpp

        core/algorithms/preprocessing/dijkstra.h
        core/algorithms/preprocessing/dijkstra.cpp
        core/algorithms/preprocessing/preprocessing.h
        core/algorithms/preprocessing/preprocessing.cpp

        core/algorithms/dynamic_programming/PW_default/PW_default.h
        core/algorithms/dynamic_programming/PW_default/PW_default.cpp
        core/algorithms/dynamic_programming/PW_default/LM_default.h
        core/algorithms/dynamic_programming/PW_default/LM_default.cpp

        core/algorithms/dynamic_programming/PW_bucket/PW_bucket.h
        core/algorithms/dynamic_programming/PW_bucket/PW_bucket.cpp
        core/algorithms/dynamic_programming/PW_bucket/LM_bucket.h
        core/algorithms/dynamic_programming/PW_bucket/LM_bucket.cpp

        core/algorithms/dynamic_programming/PW_acyclic/PW_acyclic.h
        core/algorithms/dynamic_programming/PW_acyclic/PW_acyclic.cpp
        core/algorithms/dynamic_programming/PW_acyclic/LM_acyclic.h
        core/algorithms/dynamic_programming/PW_acyclic/LM_acyclic.cpp

        core/utils/bitset.h
        core/utils/bitset.cpp
        core/utils/constants.h
        core/utils/data_collector.h
        core/utils/data_collector.cpp
        core/utils/param.h
        core/utils/param.cpp
        core/utils/logger.h
)

add_library(pathwyse_core SHARED ${SOURCE_FILES_LIB})
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set_target_properties(pathwyse_core
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ../../bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ../../bin
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ../../bin
        LIBRARY_OUTPUT_DIRECTORY ../../bin
        RUNTIME_OUTPUT_DIRECTORY ../../bin
        ARCHIVE_OUTPUT_DIRECTORY ../../bin)
else()
    set_target_properties(pathwyse_core PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../bin)
endif()

target_include_directories(pathwyse_core PUBLIC core)

set(SOURCE_FILES_STANDALONE
        standalone/main.cpp
        )

add_executable(pathwyse ${SOURCE_FILES_STANDALONE})
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set_target_properties(pathwyse PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ../../bin RUNTIME_OUTPUT_DIRECTORY ../../bin)
else()
    set_target_properties(pathwyse PROPERTIES RUNTIME_OUTPUT_DIRECTORY ../../bin)
endif()

target_link_libraries(pathwyse pathwyse_core)


