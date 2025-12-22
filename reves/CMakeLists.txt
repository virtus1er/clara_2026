cmake_minimum_required(VERSION 3.20)
project(reves VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options de compilation
add_compile_options(-Wall -Wextra -Wpedantic)

# ═══════════════════════════════════════════════════════════════════════════
# LIBRARY
# ═══════════════════════════════════════════════════════════════════════════

add_library(dream_engine STATIC
    DreamEngine.cpp
)

target_include_directories(dream_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Threads requis pour mutex
find_package(Threads REQUIRED)
target_link_libraries(dream_engine PUBLIC Threads::Threads)

# ═══════════════════════════════════════════════════════════════════════════
# TESTS
# ═══════════════════════════════════════════════════════════════════════════

enable_testing()

add_executable(dream_engine_test
    tests/DreamEngineTest.cpp
)

target_link_libraries(dream_engine_test PRIVATE dream_engine)

add_test(NAME DreamEngineTest COMMAND dream_engine_test)

# ═══════════════════════════════════════════════════════════════════════════
# EXEMPLE D'UTILISATION
# ═══════════════════════════════════════════════════════════════════════════

add_executable(dream_example
    examples/main.cpp
)

target_link_libraries(dream_example PRIVATE dream_engine)

# ═══════════════════════════════════════════════════════════════════════════
# META TARGET - builds everything
# ═══════════════════════════════════════════════════════════════════════════

add_custom_target(reves
    DEPENDS dream_engine dream_engine_test dream_example
)
