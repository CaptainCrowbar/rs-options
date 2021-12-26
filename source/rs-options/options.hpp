#pragma once

#include "rs-format/format.hpp"
#include "rs-format/string.hpp"
#include <functional>
#include <iostream>
#include <iterator>
#include <ostream>
#include <regex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace RS::Options {

    namespace Detail {

        template <typename T, typename = void> struct HasBackInserter: std::false_type {};
        template <typename T> struct HasBackInserter<T,
            std::void_t<decltype(std::inserter(std::declval<T&>(), std::declval<T&>().end()))>>:
            std::true_type {};

        template <typename T, typename = void> struct HasValueType: std::false_type {};
        template <typename T> struct HasValueType<T, std::void_t<typename T::value_type>>: std::true_type {};

        template <typename T, bool = HasValueType<T>::value> struct ValueType { using type = void; };
        template <typename T> struct ValueType<T, true> { using type = typename T::value_type; };

        template <typename T> constexpr bool is_scalar_argument_type = (
            std::is_arithmetic_v<T>
            || std::is_same_v<T, std::string>
            || (! HasBackInserter<T>::value
                && (std::is_constructible_v<T, int>
                    || std::is_constructible_v<T, std::string>))
        );

        template <typename T> constexpr bool is_container_argument_type = (
            HasBackInserter<T>::value
            && is_scalar_argument_type<typename ValueType<T>::type>
            && ! std::is_same_v<T, std::string>
        );

        template <typename T> constexpr bool is_valid_argument_type = (
            is_scalar_argument_type<T>
            || is_container_argument_type<T>
        );

    }

    // TODO
    // * Allow equals sign on long options e.g. --long-name=value
    // * Specify a pattern in add()
    // * Mutually exclusive option groups
    // * Enumeration valued options
    // * Option to accept SI tags on numbers

    class Options {

    public:

        enum flag_type: int {
            anon      = 1,  // Arguments not claimed by other options are assigned to this
            required  = 2,  // Required option
        };

        Options(const std::string& app, const std::string& version, const std::string& description,
            const std::string& extra = {});

        template <typename T> Options& add(T& var, const std::string& name, char abbrev, int flags,
            const std::string& description);
        void auto_help() noexcept { auto_help_ = true; }
        bool parse(std::vector<std::string> args, std::ostream& out = std::cout);
        bool parse(int argc, char** argv, std::ostream& out = std::cout);
        bool found(const std::string& name) const;

    private:

        using setter_type = std::function<void(const std::string&)>;

        enum class mode { boolean, single, multiple };

        struct option_info {
            setter_type setter;
            std::regex pattern;
            std::string name;
            std::string description;
            std::string placeholder;
            std::string default_value;
            char abbrev = '\0';
            mode kind = mode::single;
            bool is_anon = false;
            bool is_required = false;
            bool found = false;
        };

        std::vector<option_info> options_;
        std::string app_;
        std::string version_;
        std::string description_;
        std::string extra_;
        bool auto_help_;

        void do_add(setter_type setter, const std::regex& pattern, const std::string& name, char abbrev,
            int flags, const std::string& description, const std::string& placeholder,
            const std::string& default_value, mode kind);
        std::string format_help() const;
        size_t option_index(const std::string& name) const;
        size_t option_index(char abbrev) const;

        template <typename T> static T parse_argument(const std::string& str);
        template <typename T> static std::regex type_pattern();
        template <typename T> static std::string type_placeholder();

    };

        template <typename T>
        Options& Options::add(T& var, const std::string& name, char abbrev, int flags,
                const std::string& description) {

            using namespace Detail;
            using namespace RS::Format;

            static_assert(is_valid_argument_type<T>, "Invalid command line argument type");

            setter_type setter;
            std::regex pattern;
            std::string placeholder;
            std::string default_value;
            mode kind;

            if constexpr (std::is_same_v<T, bool>) {

                setter = [&var] (const std::string& /*str*/) { var = true; };
                kind = mode::boolean;

            } else if constexpr (is_scalar_argument_type<T>) {

                setter = [&var] (const std::string& str) { var = parse_argument<T>(str); };
                pattern = type_pattern<T>();
                placeholder = type_placeholder<T>();
                kind = mode::single;

                if ((flags & required) == 0 && var != T()) {
                    default_value = format_object(var);
                    if constexpr (! std::is_arithmetic_v<T>)
                        if (! default_value.empty())
                            default_value = quote(default_value);
                }

            } else {

                using VT = typename T::value_type;

                var.clear();
                setter = [&var] (const std::string& str) { var.insert(var.end(), parse_argument<VT>(str)); };
                pattern = type_pattern<VT>();
                placeholder = type_placeholder<VT>();
                kind = mode::multiple;

            }

            do_add(setter, pattern, name, abbrev, flags, description, placeholder, default_value, kind);

            return *this;

        }

        template <typename T>
        T Options::parse_argument(const std::string& str) {
            using namespace Detail;
            using namespace RS::Format;
            static_assert(is_scalar_argument_type<T>);
            if constexpr (std::is_same_v<T, std::string>)
                return str;
            else if constexpr (std::is_integral_v<T>)
                return to_integer<T>(str);
            else if constexpr (std::is_floating_point_v<T>)
                return to_floating<T>(str);
            else if constexpr (std::is_constructible_v<T, int>)
                return static_cast<T>(to_int64(str));
            else
                return static_cast<T>(str);
        }

        template <typename T>
        std::regex Options::type_pattern() {
            if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
                return std::regex(R"([+-]?\d+)");
            else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
                return std::regex(R"(\+?\d+)");
            else if constexpr (std::is_floating_point_v<T>)
                return std::regex(R"([+-]?(\d+(\.\d*)?|\.\d+)([Ee][+-]?\d+)?)");
            else
                return std::regex(R"(.*)");
        }

        template <typename T>
        std::string Options::type_placeholder() {
            if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
                return "<int>";
            else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
                return "<uint>";
            else if constexpr (std::is_floating_point_v<T>)
                return "<real>";
            else
                return "<arg>";
        }

}
