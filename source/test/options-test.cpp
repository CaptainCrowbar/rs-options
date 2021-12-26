#include "rs-options/options.hpp"
#include "rs-format/format.hpp"
#include "rs-format/terminal.hpp"
#include "rs-unit-test.hpp"
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace RS::Format;
using namespace RS::Options;

void test_rs_options_type_traits() {

    using namespace RS::Options::Detail;

    TEST(! HasBackInserter<void>::value);
    TEST(! HasBackInserter<int>::value);
    TEST(HasBackInserter<std::string>::value);
    TEST(HasBackInserter<std::vector<int>>::value);
    TEST(HasBackInserter<std::set<int>>::value);

    TEST(! is_scalar_argument_type<void>);
    TEST(is_scalar_argument_type<bool>);
    TEST(is_scalar_argument_type<char>);
    TEST(is_scalar_argument_type<signed char>);
    TEST(is_scalar_argument_type<unsigned char>);
    TEST(is_scalar_argument_type<short>);
    TEST(is_scalar_argument_type<unsigned short>);
    TEST(is_scalar_argument_type<int>);
    TEST(is_scalar_argument_type<unsigned>);
    TEST(is_scalar_argument_type<long>);
    TEST(is_scalar_argument_type<unsigned long>);
    TEST(is_scalar_argument_type<long long>);
    TEST(is_scalar_argument_type<unsigned long long>);
    TEST(is_scalar_argument_type<float>);
    TEST(is_scalar_argument_type<double>);
    TEST(is_scalar_argument_type<long double>);
    TEST(is_scalar_argument_type<std::string>);
    TEST(! is_scalar_argument_type<std::vector<int>>);
    TEST(! is_scalar_argument_type<std::vector<std::string>>);

    TEST(! is_container_argument_type<void>);
    TEST(! is_container_argument_type<int>);
    TEST(! is_container_argument_type<std::string>);
    TEST(is_container_argument_type<std::vector<int>>);
    TEST(is_container_argument_type<std::vector<std::string>>);

    TEST(! is_valid_argument_type<void>);
    TEST(is_valid_argument_type<int>);
    TEST(is_valid_argument_type<std::string>);
    TEST(is_valid_argument_type<std::vector<int>>);
    TEST(is_valid_argument_type<std::vector<std::string>>);

}

void test_rs_options_basic_help() {

    Options opt1("Hello", "1.0", "Says hello.", "Also says goodbye.");
    TRY(opt1.auto_help());

    {
        Options opt2 = opt1;
        TRY(opt2.set_colour(false));
        std::ostringstream out;
        TEST(! opt2.parse({}, out));
        TEST_EQUAL(out.str(),
            "\n"
            "Hello 1.0\n"
            "\n"
            "Says hello.\n"
            "\n"
            "Options:\n"
            "    --help, -h     = Show usage information\n"
            "    --version, -v  = Show version information\n"
            "\n"
            "Also says goodbye.\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        TRY(opt2.set_colour(true));
        std::ostringstream out;
        TEST(! opt2.parse({}, out));
        TEST_MATCH(out.str(),
            R"(\n)"
            R"(\x1b\[1m\x1b\[38;5;\d+mHello 1.0\x1b\[0m\n)"
            R"(\n)"
            R"(\x1b\[38;5;\d+mSays hello.\x1b\[0m\n)"
            R"(\n)"
            R"(\x1b\[38;5;\d+mOptions:\x1b\[0m\n)"
            R"(    \x1b\[38;5;\d+m--help, -h     \x1b\[38;5;\d+m= Show usage information\x1b\[0m\n)"
            R"(    \x1b\[38;5;\d+m--version, -v  \x1b\[38;5;\d+m= Show version information\x1b\[0m\n)"
            R"(\n)"
            R"(\x1b\[38;5;\d+mAlso says goodbye.\x1b\[0m\n)"
            R"(\n)"
        );
    }

}

void test_rs_options_simple_parsing() {

    std::string s = "Hello";
    int i = -123;
    unsigned u = 456;
    double d = 789.5;
    bool b = false;

    Options opt1("Hello", "", "Says hello.", "Also says goodbye.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(s, "string", 's', "String option"));
    TRY(opt1.add(i, "integer", 'i', "Integer option"));
    TRY(opt1.add(u, "unsigned", 'u', "Unsigned option"));
    TRY(opt1.add(d, "real", 'r', "Real option"));
    TRY(opt1.add(b, "boolean", 'b', "Boolean option"));

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
            "    --string, -s <arg>     = String option (default \"Hello\")\n"
            "    --integer, -i <int>    = Integer option (default -123)\n"
            "    --unsigned, -u <uint>  = Unsigned option (default 456)\n"
            "    --real, -r <real>      = Real option (default 789.5)\n"
            "    --boolean, -b          = Boolean option\n"
            "    --help, -h             = Show usage information\n"
            "    --version, -v          = Show version information\n"
            "\n"
            "Also says goodbye.\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({}, out));
        TEST_EQUAL(out.str(), "");
        TEST(! opt2.found("string"));
        TEST(! opt2.found("integer"));
        TEST(! opt2.found("unsigned"));
        TEST(! opt2.found("real"));
        TEST(! opt2.found("boolean"));
        TEST_EQUAL(s, "Hello");
        TEST_EQUAL(i, -123);
        TEST_EQUAL(u, 456u);
        TEST_EQUAL(d, 789.5);
        TEST(! b);
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "--string", "Goodbye",
            "--integer", "86",
            "--unsigned", "99",
            "--real", "42.5",
            "--boolean",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("string"));
        TEST(opt2.found("integer"));
        TEST(opt2.found("unsigned"));
        TEST(opt2.found("real"));
        TEST(opt2.found("boolean"));
        TEST_EQUAL(s, "Goodbye");
        TEST_EQUAL(i, 86);
        TEST_EQUAL(u, 99u);
        TEST_EQUAL(d, 42.5);
        TEST(b);
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "--string=Farewell",
            "--integer=123",
            "--unsigned=456",
            "--real=789",
            "--boolean",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("string"));
        TEST(opt2.found("integer"));
        TEST(opt2.found("unsigned"));
        TEST(opt2.found("real"));
        TEST(opt2.found("boolean"));
        TEST_EQUAL(s, "Farewell");
        TEST_EQUAL(i, 123);
        TEST_EQUAL(u, 456u);
        TEST_EQUAL(d, 789);
        TEST(b);
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "-s", "Hello again",
            "-i", "987",
            "-u", "654",
            "-r", "321",
            "-b",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("string"));
        TEST(opt2.found("integer"));
        TEST(opt2.found("unsigned"));
        TEST(opt2.found("real"));
        TEST(opt2.found("boolean"));
        TEST_EQUAL(s, "Hello again");
        TEST_EQUAL(i, 987);
        TEST_EQUAL(u, 654u);
        TEST_EQUAL(d, 321);
        TEST(b);
    }

}

void test_rs_options_required_options() {

    std::string s;
    int i = 0;

    Options opt1("Hello", "", "Says hello.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(s, "string", 's', "String option"));
    TRY(opt1.add(i, "integer", 'i', "Integer option", Options::required));

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
            "    --string, -s <arg>   = String option\n"
            "    --integer, -i <int>  = Integer option (required)\n"
            "    --help, -h           = Show usage information\n"
            "    --version, -v        = Show version information\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST_THROW(opt2.parse({}, out), std::invalid_argument);
        TEST_EQUAL(out.str(), "");
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "--integer", "42",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(! opt2.found("string"));
        TEST(opt2.found("integer"));
        TEST_EQUAL(s, "");
        TEST_EQUAL(i, 42);
    }

}

void test_rs_options_multiple_booleans() {

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

void test_rs_options_anonymous_options() {

    int f = 123;
    int s = 456;
    std::vector<int> r;

    Options opt1("Hello", "", "Says hello.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(f, "first", 'f', "First option", Options::anon));
    TRY(opt1.add(s, "second", 's', "Second option", Options::anon));
    TRY(opt1.add(r, "rest", 'r', "Rest of the options", Options::anon));

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
            "    [--first, -f] <int>     = First option (default 123)\n"
            "    [--second, -s] <int>    = Second option (default 456)\n"
            "    [--rest, -r] <int> ...  = Rest of the options\n"
            "    --help, -h              = Show usage information\n"
            "    --version, -v           = Show version information\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({}, out));
        TEST_EQUAL(out.str(), "");
        TEST(! opt2.found("first"));
        TEST(! opt2.found("second"));
        TEST(! opt2.found("rest"));
        TEST_EQUAL(f, 123);
        TEST_EQUAL(s, 456);
        TEST_EQUAL(format_range(r), "[]");
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({"12", "34", "56", "78", "90"}, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("first"));
        TEST(opt2.found("second"));
        TEST(opt2.found("rest"));
        TEST_EQUAL(f, 12);
        TEST_EQUAL(s, 34);
        TEST_EQUAL(format_range(r), "[56,78,90]");
    }

}

void test_rs_options_non_sequential_containers() {

    int f = 123;
    int s = 456;
    std::set<int> r;

    Options opt1("Hello", "", "Says hello.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(f, "first", 'f', "First option", Options::anon));
    TRY(opt1.add(s, "second", 's', "Second option", Options::anon));
    TRY(opt1.add(r, "rest", 'r', "Rest of the options", Options::anon));

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
            "    [--first, -f] <int>     = First option (default 123)\n"
            "    [--second, -s] <int>    = Second option (default 456)\n"
            "    [--rest, -r] <int> ...  = Rest of the options\n"
            "    --help, -h              = Show usage information\n"
            "    --version, -v           = Show version information\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "789", "789", "789", "789", "789",
            "100", "100", "100", "100", "100",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("first"));
        TEST(opt2.found("second"));
        TEST(opt2.found("rest"));
        TEST_EQUAL(f, 789);
        TEST_EQUAL(s, 789);
        TEST_EQUAL(format_range(r), "[100,789]");
    }

}

void test_rs_options_match_pattern() {

    std::string h = "Hello";
    std::string g = "Goodbye";
    std::string f = "Fubar";

    Options opt1("Hello", "", "Says hello.");
    TRY(opt1.set_colour(false));
    TRY(opt1.add(h, "hello", 'h', "Hello option", 0, "He.*"));
    TRY(opt1.add(g, "goodbye", 'g', "Goodbye option", 0, "Go.*"));
    TEST_THROW(opt1.add(f, "fubar", 'f', "Fubar option", 0, "*"), std::regex_error);
    TEST_THROW(opt1.add(f, "fubar", 'f', "Fubar option", 0, "fu.*"), std::invalid_argument);

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
            "    --hello, -h <arg>    = Hello option (default \"Hello\")\n"
            "    --goodbye, -g <arg>  = Goodbye option (default \"Goodbye\")\n"
            "    --help               = Show usage information\n"
            "    --version, -v        = Show version information\n"
            "\n"
        );
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST(opt2.parse({
            "--hello", "Hellfire",
            "--goodbye", "Godzilla",
        }, out));
        TEST_EQUAL(out.str(), "");
        TEST(opt2.found("hello"));
        TEST(opt2.found("goodbye"));
        TEST_EQUAL(h, "Hellfire");
        TEST_EQUAL(g, "Godzilla");
    }

    {
        Options opt2 = opt1;
        std::ostringstream out;
        TEST_THROW(opt2.parse({
            "--hello", "Hellfire",
            "--goodbye", "Grinch",
        }, out),
            std::invalid_argument);
    }

}
