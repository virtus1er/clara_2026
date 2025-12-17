cmake_minimum_required(VERSION 3.30)
project(emotion)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Eigen3 - try to find locally first, otherwise fetch
find_package(Eigen3 QUIET)
if(NOT Eigen3_FOUND)
    message(STATUS "Eigen3 not found locally, fetching from GitLab...")
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
endif()

# nlohmann_json - try to find locally first, otherwise fetch
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found locally, fetching from GitHub...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(Boost REQUIRED COMPONENTS system thread)

# SimpleAmqpClient
find_path(SIMPLE_AMQP_CLIENT_INCLUDE_DIR
        NAMES ../../clara_2026/emotion/SimpleAmqpClient/SimpleAmqpClient.h
        PATHS /usr/local/include /usr/include
)

find_library(SIMPLE_AMQP_CLIENT_LIBRARY
        NAMES ../../clara_2026/emotion/SimpleAmqpClient
        PATHS /usr/local/lib /usr/lib
)

if(NOT SIMPLE_AMQP_CLIENT_INCLUDE_DIR OR NOT SIMPLE_AMQP_CLIENT_LIBRARY)
    message(FATAL_ERROR "SimpleAmqpClient non trouvé. Installez-le dans /usr/local/lib et /usr/local/include.")
endif()

# ⚠️ Prioriser /usr/local pour inclure la bonne version
include_directories(BEFORE /usr/local/include)
link_directories(/usr/local/lib)

include_directories(${SIMPLE_AMQP_CLIENT_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

add_executable(27_emotion_2025 ../../clara_2026/emotion/prediction_emotions.cpp
        ../../clara_2026/emotion/prediction_emotions.cpp
        ../../clara_2026/emotion/prediction_emotions.cpp)

target_link_libraries(27_emotion_2025 PRIVATE
        Eigen3::Eigen
        nlohmann_json::nlohmann_json
        ${SIMPLE_AMQP_CLIENT_LIBRARY}
        rabbitmq
        ${Boost_LIBRARIES}
)
