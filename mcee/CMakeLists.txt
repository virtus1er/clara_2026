#===== CMakeLists.txt - Version simplifiée =====
cmake_minimum_required(VERSION 3.16)
project(MCEE VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Try to find SimpleAmqpClient
find_path(SIMPLEAMQPCLIENT_INCLUDE_DIR SimpleAmqpClient/SimpleAmqpClient.h
        PATHS /usr/local/include /usr/include)

find_library(SIMPLEAMQPCLIENT_LIBRARY SimpleAmqpClient
        PATHS /usr/local/lib /usr/lib)

# Check for nlohmann/json availability
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
endif()

# Check for RabbitMQ-C availability
pkg_check_modules(RABBITMQ_C QUIET librabbitmq)

# Decide whether to use stub based on all dependencies
if(NOT SIMPLEAMQPCLIENT_INCLUDE_DIR OR NOT SIMPLEAMQPCLIENT_LIBRARY OR
   (NOT nlohmann_json_FOUND AND NOT NLOHMANN_JSON_INCLUDE_DIR) OR
   NOT RABBITMQ_C_FOUND)
    message(STATUS "Missing dependencies, building with stub implementation:")
    if(NOT SIMPLEAMQPCLIENT_INCLUDE_DIR OR NOT SIMPLEAMQPCLIENT_LIBRARY)
        message(STATUS "  - SimpleAmqpClient not found")
    endif()
    if(NOT nlohmann_json_FOUND AND NOT NLOHMANN_JSON_INCLUDE_DIR)
        message(STATUS "  - nlohmann/json not found")
    endif()
    if(NOT RABBITMQ_C_FOUND)
        message(STATUS "  - librabbitmq not found")
    endif()
    set(USE_RABBITMQ_STUB TRUE)
else()
    message(STATUS "All dependencies found, building full implementation:")
    message(STATUS "  - SimpleAmqpClient: ${SIMPLEAMQPCLIENT_LIBRARY}")
    message(STATUS "  - RabbitMQ-C: ${RABBITMQ_C_LIBRARIES}")
    if(nlohmann_json_FOUND)
        message(STATUS "  - nlohmann/json: found via find_package")
    else()
        message(STATUS "  - nlohmann/json: ${NLOHMANN_JSON_INCLUDE_DIR}")
    endif()
    include_directories(${SIMPLEAMQPCLIENT_INCLUDE_DIR})
endif()

# Source files
set(MCEE_SOURCES
        MCEEConfig.cpp
        MCEEGradients.cpp
        MCEEContextualizer.cpp
        MCEECore.cpp
        main.cpp
)

# Conditional RabbitMQ source
if(USE_RABBITMQ_STUB)
    list(APPEND MCEE_SOURCES MCEERabbitMQ_stub.cpp)
    add_definitions(-DUSE_RABBITMQ_STUB)
else()
    list(APPEND MCEE_SOURCES MCEERabbitMQ.cpp)
endif()

# Header files
set(MCEE_HEADERS
        MCEETypes.h
        MCEEConfig.h
        MCEERabbitMQ.h
        MCEEGradients.h
        MCEEContextualizer.h
        MCEECore.h
)

# Create executable
add_executable(mcee ${MCEE_SOURCES} ${MCEE_HEADERS})

# Link libraries
target_link_libraries(mcee
        pthread
)

# Conditional linking
if(NOT USE_RABBITMQ_STUB)
    target_link_libraries(mcee ${SIMPLEAMQPCLIENT_LIBRARY})
endif()

# Link RabbitMQ-C if not using stub and if found
if(NOT USE_RABBITMQ_STUB AND RABBITMQ_C_FOUND)
    target_link_libraries(mcee ${RABBITMQ_C_LIBRARIES})
    target_include_directories(mcee PRIVATE ${RABBITMQ_C_INCLUDE_DIRS})
    target_compile_definitions(mcee PRIVATE ${RABBITMQ_C_CFLAGS_OTHER})
endif()

# Link nlohmann/json if not using stub
if(NOT USE_RABBITMQ_STUB)
    if(nlohmann_json_FOUND)
        target_link_libraries(mcee nlohmann_json::nlohmann_json)
    elseif(NLOHMANN_JSON_INCLUDE_DIR)
        target_include_directories(mcee PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
    endif()
endif()

# Install target
install(TARGETS mcee DESTINATION bin)
install(FILES mcee_config.txt DESTINATION etc)

# Copy config file to build directory for development
configure_file(${CMAKE_SOURCE_DIR}/mcee_config.txt
        ${CMAKE_BINARY_DIR}/mcee_config.txt COPYONLY)