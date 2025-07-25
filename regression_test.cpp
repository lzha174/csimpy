#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "csimpy_env.h"
#include "examples.h"
#include <sstream>
#include <iostream>

TEST_CASE("example_1 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    // Run example_1 (no env parameter)
    example_1();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[0] process_c started\n"
        "[0] process_b started\n"
        "[0] process_a started\n"
        "[5] process_a now waiting on process_c\n"
        "[10] process_b now waiting on process_c\n"
        "[15] process_c finished\n"
        "[15] process_a resumed after process_c\n"
        "[15] process_b resumed after process_c\n"
        "[40] process_a finished \n"
        "[40] proc_b finished waiting allofevent\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_2 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_2();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[5] test_put_first: putting 4\n"
        "[5] test_put_first: done\n"
        "[6] test_get_second: trying to get 3 current level  4\n"
        "[6] test_get_second: got 3 current level  1\n"
        "[6] test_get_second: trying to get 9 current level  1\n"
        "[10] test_put_first: putting 10\n"
        "[10] test_put_first: done\n"
        "[10] test_get_second: got 9 current level  2\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_3 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_3();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[10] All delays finished.\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_4 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_4();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[1] task1 waiting on shared_event or timeout\n"
        "[10] task2 triggering shared_event\n"
        "[10] task1 finished waiting (timeout and event)\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_patient_flow regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_patient_flow();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[0] patient starts registration\n"
        "[10] patient finishes registration\n"
        "[10] patient starts seeing doctor\n"
        "[10] patient starts lab test\n"
        "[30] patient finishes seeing doctor\n"
        "[50] patient finishes lab test\n"
        "[50] patient signs out\n";

    CHECK_EQ(output, expected);
}