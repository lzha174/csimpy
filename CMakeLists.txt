cmake_minimum_required(VERSION 3.31)
project(csimpy)

set(CMAKE_CXX_STANDARD 20)


add_executable(csimpy
        main.cpp
        csimpy_env.cpp
        csimpy_delay.cpp
)
