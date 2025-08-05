
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../../include/csimpy/csimpy_env.h"
#include "../../include//examples/examples.h"
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
        "[5] test_put_first: done current level 4\n"
        "[6] test_get_second: trying to get 3 current level  4\n"
        "[6] test_get_second: got 3 current level  1\n"
        "[6] test_get_second: trying to get 9 current level  1\n"
        "[10] test_put_first: putting 10\n"
        "[10] test_put_first: done current level 2\n"
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

TEST_CASE("example_5 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_5();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[0] proc_any_wait started\n"
        "[5] AnyOfEvent triggered after one delay\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_6 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_6();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[0] proc_b started\n"
        "[0] proc_b waiting on proc_a or 10 delay\n"
        "[0] proc_a started\n"
        "[5] proc_a finished\n"
        "[5] proc_b resumed after AnyOfEvent\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_7 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_7();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[0] proc_b started\n"
        "[10] proc_b finished delay, now scheduling proc_a\n"
        "[10] proc_a started\n"
        "[15] proc_a finished\n";

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

TEST_CASE("example_8 regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_8();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[1] Putting Alice\n"
        "[1] Putting Bob\n"
        "[1] Getting item with id == 2\n"
        "[1] Got item with id == StaffItem(Bob, id=2, role=Doctor, skill=3)\n"
        "[1] Getting next available item (no filter)\n"
        "[1] Got item: StaffItem(Alice, id=1, role=Nurse, skill=2)\n";

    CHECK_EQ(output, expected);
}

TEST_CASE("example_carwash regression") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    example_carwash_with_container();

    std::cout.rdbuf(old);
    std::string output = buffer.str();

    const char* expected =
        "[0] Car 0 arrives at the carwash.\n"
        "[0] Car 1 arrives at the carwash.\n"
        "[0] Car 2 arrives at the carwash.\n"
        "[0] Car 3 arrives at the carwash.\n"
        "[0] Car 0 enters the carwash.\n"
        "[0] Car 1 enters the carwash.\n"
        "[5] Car 4 arrives at the carwash.\n"
        "[10] Car 0 leaves the carwash.\n"
        "[10] Car 1 leaves the carwash.\n"
        "[10] Car 5 arrives at the carwash.\n"
        "[10] Car 2 enters the carwash.\n"
        "[10] Car 3 enters the carwash.\n"
        "[15] Car 6 arrives at the carwash.\n"
        "[20] Car 2 leaves the carwash.\n"
        "[20] Car 3 leaves the carwash.\n"
        "[20] Car 7 arrives at the carwash.\n"
        "[20] Car 4 enters the carwash.\n"
        "[20] Car 5 enters the carwash.\n"
        "[25] Car 8 arrives at the carwash.\n"
        "[30] Car 4 leaves the carwash.\n"
        "[30] Car 5 leaves the carwash.\n"
        "[30] Car 6 enters the carwash.\n"
        "[30] Car 7 enters the carwash.\n"
        "[40] Car 6 leaves the carwash.\n"
        "[40] Car 7 leaves the carwash.\n"
        "[40] Car 8 enters the carwash.\n"
        "[50] Car 8 leaves the carwash.\n";

    CHECK_EQ(output, expected);
}