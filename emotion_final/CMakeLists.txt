cmake_minimum_required(VERSION 3.14)
project(emotion)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch Eigen3
FetchContent_Declare(
        Eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3.4.0
        GIT_SHALLOW TRUE
)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Eigen3)

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
    message(FATAL_ERROR "SimpleAmqpClient not found. Please install it to /usr/local/lib and /usr/local/include.")
endif()

# Prioritize /usr/local for correct versions
include_directories(BEFORE /usr/local/include)
link_directories(/usr/local/lib)

include_directories(${SIMPLE_AMQP_CLIENT_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

add_executable(emotion_prediction main.cpp)

target_link_libraries(emotion_prediction PRIVATE
        Eigen3::Eigen
        nlohmann_json::nlohmann_json
        ${SIMPLE_AMQP_CLIENT_LIBRARY}
        rabbitmq
        ${Boost_LIBRARIES}
)
