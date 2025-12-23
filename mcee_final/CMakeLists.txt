cmake_minimum_required(VERSION 3.14)
project(mcee VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Fetch nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# Find Boost (system install required)
find_package(Boost REQUIRED COMPONENTS system thread)

# SimpleAmqpClient (system install required)
find_path(SIMPLE_AMQP_CLIENT_INCLUDE_DIR
    NAMES SimpleAmqpClient/SimpleAmqpClient.h
    PATHS /usr/local/include /usr/include
)

find_library(SIMPLE_AMQP_CLIENT_LIBRARY
    NAMES SimpleAmqpClient
    PATHS /usr/local/lib /usr/lib
)

if(NOT SIMPLE_AMQP_CLIENT_INCLUDE_DIR OR NOT SIMPLE_AMQP_CLIENT_LIBRARY)
    message(FATAL_ERROR "SimpleAmqpClient not found. Please install it.")
endif()

# Prioritize /usr/local for correct versions
include_directories(BEFORE /usr/local/include)
link_directories(/usr/local/lib)

include_directories(${SIMPLE_AMQP_CLIENT_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

# Source files - Version 3.0 avec MCT/MLT + Neo4j + MCTGraph + Conscience + ADDO
set(MCEE_SOURCES
    src/main.cpp
    src/MCEEEngine.cpp
    src/MCT.cpp
    src/MCTGraph.cpp
    src/MLT.cpp
    src/PatternMatcher.cpp
    src/PhaseDetector.cpp
    src/EmotionUpdater.cpp
    src/Amyghaleon.cpp
    src/MemoryManager.cpp
    src/SpeechInput.cpp
    src/Neo4jClient.cpp
    src/ConscienceEngine.cpp
    src/ADDOEngine.cpp
)

set(MCEE_HEADERS
    include/Types.hpp
    include/MCEEEngine.hpp
    include/MCT.hpp
    include/MCTGraph.hpp
    include/MLT.hpp
    include/PhaseDetector.hpp
    include/EmotionUpdater.hpp
    include/Amyghaleon.hpp
    include/MemoryManager.hpp
    include/PhaseConfig.hpp
    include/SpeechInput.hpp
    include/PatternMatcher.hpp
    include/Neo4jClient.hpp
    include/ConscienceConfig.hpp
    include/ConscienceEngine.hpp
    include/ADDOConfig.hpp
    include/ADDOEngine.hpp
)

add_executable(mcee ${MCEE_SOURCES} ${MCEE_HEADERS})

target_include_directories(mcee PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(mcee PRIVATE
    nlohmann_json::nlohmann_json
    ${SIMPLE_AMQP_CLIENT_LIBRARY}
    rabbitmq
    ${Boost_LIBRARIES}
)

# Copy config file to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/phase_config.json
    ${CMAKE_CURRENT_BINARY_DIR}/phase_config.json
    COPYONLY
)
