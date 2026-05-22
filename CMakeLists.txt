cmake_minimum_required(VERSION 3.16)
project(rinha-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Find nlohmann/json - try multiple methods
find_package(nlohmann_json 3.2.0)
if(NOT nlohmann_json_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSON nlohmann_json3)
endif()

# HNSW library
set(HNSWLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/hnswlib)
include_directories(${HNSWLIB_DIR})

# Source files
set(SOURCES
    src/main.cpp
    src/vectorizer.cpp
    src/index_loader.cpp
    src/http_server.cpp
    src/metadata.cpp
)

set(HEADERS
    src/vectorizer.h
    src/index_loader.h
    src/http_server.h
    src/metadata.h
)

# Executable
add_executable(rinha-api ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(rinha-api
    PRIVATE
    Threads::Threads
    microhttpd
    z
)

if(nlohmann_json_FOUND)
    target_link_libraries(rinha-api PRIVATE nlohmann_json::nlohmann_json)
elseif(JSON_FOUND)
    target_link_libraries(rinha-api PRIVATE ${JSON_LIBRARIES})
    target_include_directories(rinha-api PRIVATE ${JSON_INCLUDE_DIRS})
endif()

# Include directories
target_include_directories(rinha-api
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/hnswlib
)

# hnswlib is header-only, no .cpp files to compile
