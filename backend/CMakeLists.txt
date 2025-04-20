# Minimum CMake version required
cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project name
project(CTSync-server LANGUAGES C)
set(CMAKE_BUILD_TYPE Debug)


add_subdirectory(external/cequiq)
include_directories(external/cequiq/include)
include_directories(./include)

# Package finding
find_package(SQLite3 REQUIRED)
find_library(UUID_LIB uuid)
find_package(cJSON REQUIRED)

# Add the executable
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable(CTSync-server ${SRC_FILES})

# Link the libraries
target_link_libraries(CTSync-server 
    cequiq
    SQLite::SQLite3 
    uuid
    cjson
)
