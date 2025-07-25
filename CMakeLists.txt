cmake_minimum_required(VERSION 3.31)
project(csimpy)

set(CMAKE_CXX_STANDARD 20)

# Add the Catch2 include path
include_directories(/usr/local/Cellar/catch2/3.9.0/include)
include_directories(/usr/local/Cellar/doctest/2.4.12/include)

find_package(doctest REQUIRED)

# Main executable for simulator
add_executable(csimpy
        main.cpp
        csimpy_env.cpp
        examples.cpp
)

#


