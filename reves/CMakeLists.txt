cmake_minimum_required(VERSION 3.20)
project(reves VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options de compilation
add_compile_options(-Wall -Wextra -Wpedantic)

# Options
option(REVES_WITH_RABBITMQ "Build with RabbitMQ support" OFF)
option(REVES_BUILD_EXAMPLES "Build examples" ON)
option(REVES_BUILD_TESTS "Build tests" ON)

# ═══════════════════════════════════════════════════════════════════════════
# DÉPENDANCES
# ═══════════════════════════════════════════════════════════════════════════

# nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Threads
find_package(Threads REQUIRED)

# RabbitMQ (optionnel)
if(REVES_WITH_RABBITMQ)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SIMPLEAMQPCLIENT REQUIRED SimpleAmqpClient)
    add_definitions(-DREVES_HAS_RABBITMQ)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# LIBRARY - DreamEngine (core)
# ═══════════════════════════════════════════════════════════════════════════

add_library(dream_engine STATIC
    DreamEngine.cpp
)

target_include_directories(dream_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dream_engine PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# ═══════════════════════════════════════════════════════════════════════════
# LIBRARY - DreamIntegration (avec RabbitMQ)
# ═══════════════════════════════════════════════════════════════════════════

if(REVES_WITH_RABBITMQ)
    add_library(dream_integration INTERFACE)
    
    target_include_directories(dream_integration INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SIMPLEAMQPCLIENT_INCLUDE_DIRS}
    )
    
    target_link_libraries(dream_integration INTERFACE
        dream_engine
        ${SIMPLEAMQPCLIENT_LIBRARIES}
    )
endif()

# ═══════════════════════════════════════════════════════════════════════════
# EXÉCUTABLE PRINCIPAL - target "reves"
# ═══════════════════════════════════════════════════════════════════════════

add_executable(reves
    examples/main.cpp
)

target_link_libraries(reves PRIVATE dream_engine)

# ═══════════════════════════════════════════════════════════════════════════
# EXEMPLES
# ═══════════════════════════════════════════════════════════════════════════

if(REVES_BUILD_EXAMPLES)
    # Exemple simple (sans RabbitMQ)
    add_executable(dream_simple_example
        examples/simple_example.cpp
    )
    target_link_libraries(dream_simple_example PRIVATE dream_engine)
    
    # Exemple d'intégration (nécessite RabbitMQ)
    if(REVES_WITH_RABBITMQ)
        add_executable(dream_integration_example
            examples/integration_example.cpp
        )
        target_link_libraries(dream_integration_example PRIVATE 
            dream_integration
        )
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════
# TESTS
# ═══════════════════════════════════════════════════════════════════════════

if(REVES_BUILD_TESTS)
    enable_testing()
    
    add_executable(dream_engine_test
        tests/DreamEngineTest.cpp
    )
    target_link_libraries(dream_engine_test PRIVATE dream_engine)
    add_test(NAME DreamEngineTest COMMAND dream_engine_test)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# INFO BUILD
# ═══════════════════════════════════════════════════════════════════════════

message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "  Module Rêves - Configuration")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "  C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  RabbitMQ Support:  ${REVES_WITH_RABBITMQ}")
message(STATUS "  Build Examples:    ${REVES_BUILD_EXAMPLES}")
message(STATUS "  Build Tests:       ${REVES_BUILD_TESTS}")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "")
