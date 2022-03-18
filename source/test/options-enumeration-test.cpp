#include "rs-options/options.hpp"
#include "rs-format/format.hpp"
#include "rs-tl/enum.hpp"
#include "rs-unit-test.hpp"
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace RS::Format;
using namespace RS::Option;

RS_DEFINE_ENUM_CLASS(MyEnum, int, 0, alpha, bravo, charlie)

void test_rs_options_enumeration() {

    MyEnum m = MyEnum::alpha;
    std::vector<MyEnum> v;

    Options opt1("Hello", "", "Says hello.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(m, "my-enum", 'm', "My enum option"));
    TRY(opt1.add(v, "enum-vector", 'v', "Enum vector option"));

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
            "    --my-enum, -m <arg>          = My enum option (default alpha)\n"
            "    --enum-vector, -v <arg> ...  = Enum vector option\n"
            "    --help, -h                   = Show usage information\n"
            "    --version                    = Show version information\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({}, out));
        TEST_EQUAL(out.str(), "");
        TEST(! opt2.found("my-enum"));
        TEST(! opt2.found("enum-vector"));
        TEST_EQUAL(m, MyEnum::alpha);
        TEST_EQUAL(format_range(v), "[]");
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "--my-enum", "bravo",
            "--enum-vector", "alpha", "bravo", "charlie",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("my-enum"));
        TEST(opt2.found("enum-vector"));
        TEST_EQUAL(m, MyEnum::bravo);
        TEST_EQUAL(format_range(v), "[alpha,bravo,charlie]");
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST_THROW_MATCH(opt2.parse({
            "--my-enum", "delta",
            "--enum-vector", "alpha", "bravo", "charlie",
        }, out),
            Options::user_error, "delta");
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST_THROW_MATCH(opt2.parse({
            "--my-enum", "bravo",
            "--enum-vector", "alpha", "bravo", "charlie", "delta",
        }, out),
            Options::user_error, "delta");
    }

}
