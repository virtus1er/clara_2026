cmake_minimum_required(VERSION 3.20)
project(reves VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ═══════════════════════════════════════════════════════════════════════════
# OPTIONS
# ═══════════════════════════════════════════════════════════════════════════

option(BUILD_MAIN "Build main executable (requires RabbitMQ)" ON)
option(BUILD_TESTS "Build test executable" ON)

# ═══════════════════════════════════════════════════════════════════════════
# DÉPENDANCES COMMUNES
# ═══════════════════════════════════════════════════════════════════════════

# Threads
find_package(Threads REQUIRED)

# ═══════════════════════════════════════════════════════════════════════════
# EXÉCUTABLE PRINCIPAL (nécessite RabbitMQ)
# ═══════════════════════════════════════════════════════════════════════════

if(BUILD_MAIN)
    # nlohmann/json
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)

    # RabbitMQ
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RABBITMQ REQUIRED librabbitmq)

    # SimpleAmqpClient
    find_library(SIMPLEAMQPCLIENT_LIB SimpleAmqpClient REQUIRED)

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

    install(TARGETS reves DESTINATION bin)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# TESTS (pas besoin de RabbitMQ)
# ═══════════════════════════════════════════════════════════════════════════

if(BUILD_TESTS)
    enable_testing()

    add_executable(dream_tests
        tests/DreamEngineTest.cpp
        DreamEngine.cpp
    )

    target_include_directories(dream_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(dream_tests PRIVATE
        Threads::Threads
    )

    add_test(NAME DreamEngineTests COMMAND dream_tests)
endif()
