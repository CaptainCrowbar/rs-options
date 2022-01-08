#include "rs-options/options.hpp"
#include "rs-unit-test.hpp"
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace RS::Option;

void test_rs_options_boolean() {

    bool a = false;
    bool b = false;
    bool c = false;

    Options opt1("Hello", "", "Says hello.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(a, "alpha", 'a', "Alpha option"));
    TRY(opt1.add(b, "bravo", 'b', "Bravo option"));
    TRY(opt1.add(c, "charlie", 'c', "Charlie option"));

    {
        Options opt2 = opt1;
        TRY(opt2.auto_help());
        std::ostringstream out;
        TEST(! opt2.parse({}, out));
        TEST_EQUAL(out.str(),
            "\n"
            "Hello\n"
            "\n"
            "Says hello.\n"
            "\n"
            "Options:\n"
            "    --alpha, -a    = Alpha option\n"
            "    --bravo, -b    = Bravo option\n"
            "    --charlie, -c  = Charlie option\n"
            "    --help, -h     = Show usage information\n"
            "    --version, -v  = Show version information\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "-abc",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("alpha"));
        TEST(opt2.found("bravo"));
        TEST(opt2.found("charlie"));
        TEST(a);
        TEST(b);
        TEST(c);
    }

}
