cmake_minimum_required(VERSION 3.15)
project(csimpy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

# Source files used by both executables
set(CSIMPY_SOURCES
        csimpy_env.cpp
        examples.cpp
)

# ---- Main executable ----
add_executable(csimpy_main
        main.cpp
        ${CSIMPY_SOURCES}
)

add_executable(csimpy_play
        play.cpp
        ${CSIMPY_SOURCES}
)

# ---- Regression test executable ----
add_executable(csimpy_tests
        regression_test.cpp
        ${CSIMPY_SOURCES}
)

# Include doctest for tests
find_package(doctest REQUIRED)
target_link_libraries(csimpy_tests PRIVATE doctest::doctest)




add_custom_target(csimpy ALL
        DEPENDS csimpy_main csimpy_tests
)


