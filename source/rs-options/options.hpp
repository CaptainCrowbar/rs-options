#pragma once

#include "rs-format/enum.hpp"
#include "rs-format/format.hpp"
#include "rs-format/string.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <ostream>
#include <regex>
#include <stdexcept>
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
            || std::is_enum_v<T>
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

    class Options {

    public:

        enum flag_type: int {
            anon      = 1,  // Arguments not claimed by other options are assigned to this
            required  = 2,  // Required option
        };

        Options() = default;
        Options(const std::string& app, const std::string& version,
            const std::string& description, const std::string& extra = {});

        template <typename T> Options& add(T& var, const std::string& name, char abbrev, const std::string& description,
            int flags = 0, const std::string& group = {}, const std::string& pattern = {});
        void auto_help() noexcept { auto_help_ = true; }
        void set_colour(bool b) noexcept { colour_ = int(b); }
        bool parse(std::vector<std::string> args, std::ostream& out = std::cout);
        bool parse(int argc, char** argv, std::ostream& out = std::cout);
        bool found(const std::string& name) const;

    private:

        using setter_type = std::function<void(const std::string&)>;
        using validator_type = std::function<bool(const std::string&)>;

        enum class mode { boolean, single, multiple };

        struct option_info {
            setter_type setter;
            validator_type validator;
            std::string name;
            std::string description;
            std::string placeholder;
            std::string default_value;
            std::string group;
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
        int colour_ = 0;
        bool auto_help_ = false;

        void do_add(setter_type setter, validator_type validator, const std::string& name, char abbrev,
            const std::string& description, const std::string& placeholder, const std::string& default_value,
            mode kind, int flags, const std::string& group);
        std::string format_help() const;
        std::string group_list(const std::string& group) const;
        size_t option_index(const std::string& name) const;
        size_t option_index(char abbrev) const;

        template <typename T> static T parse_argument(const std::string& arg);
        template <typename T> static validator_type type_validator(const std::string& name, const std::string& pattern);
        template <typename T> static std::string type_placeholder();

    };

        template <typename T>
        Options& Options::add(T& var, const std::string& name, char abbrev, const std::string& description,
                int flags, const std::string& group, const std::string& pattern) {

            using namespace Detail;
            using namespace RS::Format;
            using namespace RS::Format::Literals;

            static_assert(is_valid_argument_type<T>, "Invalid command line argument type");

            setter_type setter;
            validator_type validator;
            std::string placeholder;
            std::string default_value;
            mode kind;

            if constexpr (std::is_same_v<T, bool>) {

                setter = [&var] (const std::string& /*str*/) { var = true; };
                kind = mode::boolean;

            } else if constexpr (is_scalar_argument_type<T>) {

                setter = [&var] (const std::string& str) { var = parse_argument<T>(str); };
                validator = type_validator<T>(name, pattern);
                placeholder = type_placeholder<T>();
                kind = mode::single;

                if constexpr (std::is_same_v<T, std::string>)
                    if (validator && ! validator(var))
                        throw std::invalid_argument("Default value does not match pattern: --" + name);

                if ((flags & required) == 0 && (std::is_enum_v<T> || var != T())) {
                    default_value = format_object(var);
                    if constexpr (! std::is_arithmetic_v<T> && ! std::is_enum_v<T>)
                        if (! default_value.empty())
                            default_value = quote(default_value);
                }

            } else {

                using VT = typename T::value_type;

                var.clear();
                setter = [&var] (const std::string& str) { var.insert(var.end(), parse_argument<VT>(str)); };
                validator = type_validator<VT>(name, pattern);
                placeholder = type_placeholder<VT>();
                kind = mode::multiple;

            }

            do_add(setter, validator, name, abbrev, description, placeholder, default_value, kind, flags, group);

            return *this;

        }

        template <typename T>
        T Options::parse_argument(const std::string& arg) {
            using namespace Detail;
            using namespace RS::Format;
            using namespace RS::Format::Literals;
            static_assert(is_scalar_argument_type<T>);
            if constexpr (std::is_enum_v<T>) {
                T t;
                if (! parse_enum(arg, t))
                    throw std::invalid_argument("Invalid enumeration value: {0:q}"_fmt(arg));
                return t;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_integral_v<T>) {
                return to_integer<T>(arg);
            } else if constexpr (std::is_floating_point_v<T>) {
                return to_floating<T>(arg);
            } else if constexpr (std::is_constructible_v<T, int>) {
                return static_cast<T>(to_int64(arg));
            } else {
                return static_cast<T>(arg);
            }
        }

        template <typename T>
        Options::validator_type Options::type_validator(const std::string& name, const std::string& pattern) {

            using namespace RS::Format::Literals;

            validator_type validator;
            std::optional<std::regex> re;

            if (! pattern.empty()) {
                if constexpr (! std::is_same_v<T, std::string>)
                    throw std::invalid_argument("Pattern is only allowed for string-valued options: {0:q}"_fmt("--" + name));
                re = std::regex(pattern);
            }

            if constexpr (std::is_enum_v<T>)
                validator = [] (const std::string& str) {
                    auto& names = list_enum_names(T());
                    return std::find(names.begin(), names.end(), str) != names.end();
                };
            else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
                re = std::regex(R"([+-]?\d+)");
            else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
                re = std::regex(R"(\+?\d+)");
            else if constexpr (std::is_floating_point_v<T>)
                re = std::regex(R"([+-]?(\d+(\.\d*)?|\.\d+)([Ee][+-]?\d+)?)");

            if (re)
                validator = [re] (const std::string& str) { return std::regex_match(str, *re); };

            return validator;

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
