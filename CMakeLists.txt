cmake_minimum_required(VERSION 3.20)
project(GradleDependencyUpdater VERSION 1.0.0 LANGUAGES CXX)

# Enforce modern C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# CPM.cmake Auto-Download Setup
# ---------------------------------------------------------------------------
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DIR "${CMAKE_BINARY_DIR}/cmake")

if(NOT EXISTS "${CPM_DIR}/CPM.cmake")
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}...")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        "${CPM_DIR}/CPM.cmake"
        SHOW_PROGRESS
    )
endif()
include("${CPM_DIR}/CPM.cmake")

# ---------------------------------------------------------------------------
# External Dependencies via CPM
# ---------------------------------------------------------------------------

# 1. nlohmann_json: For REST API request/response JSON serialization
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
)

# 2. CPR (C++ Requests): Modern C++ wrapper around libcurl for REST calls
CPMAddPackage(
    NAME cpr
    GITHUB_REPOSITORY libcpr/cpr
    VERSION 1.10.5
    GIT_TAG 1.10.5   # <--- Add this explicitly
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "CPR_ENABLE_SSL ON"
)

# ---------------------------------------------------------------------------
# Executable & Include Target
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
)
