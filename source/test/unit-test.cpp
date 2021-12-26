#include "rs-unit-test.hpp"

int main(int argc, char** argv) {

    RS::UnitTest::begin_tests(argc, argv);

    // version-test.cpp
    UNIT_TEST(skeleton_version)

    // options-test.cpp
    UNIT_TEST(rs_options_type_traits)
    UNIT_TEST(rs_options_basic_help)
    UNIT_TEST(rs_options_simple_parsing)
    UNIT_TEST(rs_options_required_options)
    UNIT_TEST(rs_options_multiple_booleans)
    UNIT_TEST(rs_options_anonymous_options)
    UNIT_TEST(rs_options_non_sequential_containers)
    UNIT_TEST(rs_options_match_pattern)

    // unit-test.cpp

    return RS::UnitTest::end_tests();

}
