cmake_minimum_required(VERSION 3.20)
project(reves VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ═══════════════════════════════════════════════════════════════════════════
# DÉPENDANCES
# ═══════════════════════════════════════════════════════════════════════════

# nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Threads
find_package(Threads REQUIRED)

# RabbitMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(RABBITMQ REQUIRED librabbitmq)

# SimpleAmqpClient
find_library(SIMPLEAMQPCLIENT_LIB SimpleAmqpClient REQUIRED)

# ═══════════════════════════════════════════════════════════════════════════
# EXÉCUTABLE
# ═══════════════════════════════════════════════════════════════════════════

add_executable(reves
    main.cpp
    DreamEngine.cpp
)

target_include_directories(reves PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RABBITMQ_INCLUDE_DIRS}
)

target_link_libraries(reves PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${SIMPLEAMQPCLIENT_LIB}
    ${RABBITMQ_LIBRARIES}
)

# ═══════════════════════════════════════════════════════════════════════════
# INSTALL
# ═══════════════════════════════════════════════════════════════════════════

install(TARGETS reves DESTINATION bin)
