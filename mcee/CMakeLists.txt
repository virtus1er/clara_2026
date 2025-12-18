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

# Try to find SimpleAmqpClient first
find_path(SIMPLEAMQPCLIENT_INCLUDE_DIR SimpleAmqpClient/SimpleAmqpClient.h
        PATHS /usr/local/include /usr/include)

find_library(SIMPLEAMQPCLIENT_LIBRARY SimpleAmqpClient
        PATHS /usr/local/lib /usr/lib)

if(NOT SIMPLEAMQPCLIENT_INCLUDE_DIR OR NOT SIMPLEAMQPCLIENT_LIBRARY)
    message(STATUS "SimpleAmqpClient not found, building fallback version...")
    # Build without SimpleAmqpClient - create a stub version
    set(USE_RABBITMQ_STUB TRUE)
else()
    message(STATUS "Found SimpleAmqpClient: ${SIMPLEAMQPCLIENT_LIBRARY}")
    include_directories(${SIMPLEAMQPCLIENT_INCLUDE_DIR})
endif()

# Only require RabbitMQ-C and nlohmann/json if not using stub
if(NOT USE_RABBITMQ_STUB)
    # Find RabbitMQ-C
    pkg_check_modules(RABBITMQ_C REQUIRED librabbitmq)

    # Try to find nlohmann/json
    find_package(nlohmann_json 3.2.0 QUIET)
    if(NOT nlohmann_json_FOUND)
        # Fallback: check if header is available
        find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
        if(NOT NLOHMANN_JSON_INCLUDE_DIR)
            message(FATAL_ERROR "nlohmann/json not found. Install with: sudo apt install nlohmann-json3-dev")
        endif()
    endif()
else()
    message(STATUS "Using RabbitMQ stub, librabbitmq and nlohmann/json not required")
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