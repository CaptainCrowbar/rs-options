#include "rs-options/options.hpp"
#include "rs-format/terminal.hpp"
#include <algorithm>
#include <stdexcept>

using namespace RS::Format;
using namespace RS::Format::Literals;

namespace RS::Options {

    namespace {

        std::string trim_name(const std::string& name) {
            return trim(name, std::string(ascii_whitespace) + '-');
        }

    }

    Options::Options(const std::string& app, const std::string& version,
        const std::string& description, const std::string& extra):
    options_(),
    app_(trim(app)),
    version_(trim(version)),
    description_(trim(description)),
    extra_(trim(extra)),
    colour_(-1),
    auto_help_(false) {
        if (app.empty())
            throw std::invalid_argument("No application name was supplied");
        if (description.empty())
            throw std::invalid_argument("No application description was supplied");
        if (! version_.empty())
            version_.insert(0, 1, ' ');
    }

    bool Options::parse(std::vector<std::string> args, std::ostream& out) {

        char help_abbrev = option_index('h') == npos ? 'h' : '\0';
        char version_abbrev = option_index('v') == npos ? 'v' : '\0';
        bool want_help = false;
        bool want_version = false;
        add(want_help, "help", help_abbrev, "Show usage information");
        add(want_version, "version", version_abbrev, "Show version information");

        if (auto_help_ && args.empty()) {
            out << format_help();
            return false;
        }

        option_info* current = nullptr;
        size_t i = 0;
        bool escaped = false;

        auto mark_found = [&current] (option_info& opt) {
            current = &opt;
            opt.found = true;
            if (opt.kind == mode::boolean)
                opt.setter({});
        };

        while (i < args.size()) {

            auto& arg = args[i];

            if (escaped || arg[0] != '-') {

                // Argument to an option

                if (current == nullptr) {
                    auto it = std::find_if(options_.begin(), options_.end(), [] (auto& opt) {
                        return opt.is_anon && (opt.kind == mode::multiple || ! opt.found);
                    });
                    if (it == options_.end())
                        throw std::invalid_argument("Argument not associated with an option: {0:q}"_fmt(arg));
                    mark_found(*it);
                }

                if (! std::regex_match(arg, current->pattern))
                    throw std::invalid_argument("Argument does not match expected pattern: {0:q}"_fmt(arg));

                current->setter(arg);
                if (current->kind != mode::multiple)
                    current = nullptr;
                ++i;

            } else if (arg == "--") {

                // Remaining arguments can't be options

                escaped = true;
                ++i;

            } else if (arg[0] == '-' && arg[1] == '-') {

                size_t eq_pos = arg.find('=');

                if (eq_pos >= 4 && eq_pos != npos) {

                    // Long option name and value combined

                    auto suffix = arg.substr(eq_pos + 1);
                    arg.resize(eq_pos);
                    args.insert(args.begin() + i + 1, suffix);

                } else {

                    // Long option name

                    size_t j = option_index(arg.substr(2));
                    if (j == npos)
                        throw std::invalid_argument("Unknown option: {0:q}"_fmt(arg));
                    mark_found(options_[j]);
                    ++i;

                }

            } else if (arg.size() > 2) {

                // Multiple short options

                std::vector<std::string> new_args;
                for (char c: arg.substr(1))
                    new_args.push_back({'-', c});
                auto it = args.begin() + i;
                args.erase(it);
                args.insert(it, new_args.begin(), new_args.end());

            } else {

                // Short option name

                size_t j = option_index(arg[1]);
                if (j == npos)
                    throw std::invalid_argument("Unknown option: {0:q}"_fmt(arg));
                mark_found(options_[j]);
                ++i;

            }

        }

        auto it = std::find_if(options_.begin(), options_.end(), [] (auto& opt) { return opt.is_required && ! opt.found; });
        if (it != options_.end())
            throw std::invalid_argument("Required option not found: {0:q}"_fmt("--" + it->name));

        size_t index = option_index("help");
        if (options_[index].found) {
            out << format_help();
            return false;
        }

        index = option_index("version");
        if (options_[index].found) {
            out << app_ << version_ << "\n";
            return false;
        }

        return true;

    }

    bool Options::parse(int argc, char** argv, std::ostream& out) {
        std::vector<std::string> args(argv + 1, argv + argc);
        return parse(args, out);
    }

    bool Options::found(const std::string& name) const {
        auto i = option_index(name);
        return i != npos && options_[i].found;
    }

    void Options::do_add(setter_type setter, const std::regex& pattern, const std::string& name, char abbrev,
            const std::string& description, const std::string& placeholder, const std::string& default_value,
            mode kind, int flags) {

        bool anon_complete = false;
        option_info info;

        info.setter = setter;
        info.pattern = pattern;
        info.name = trim_name(name);
        info.description = trim(description);
        info.placeholder = placeholder;
        info.default_value = default_value;
        info.abbrev = abbrev;
        info.kind = kind;
        info.is_anon = (flags & anon) != 0;
        info.is_required = (flags & required) != 0;

        std::string long_name = "--" + info.name;
        std::string short_name = {'-', info.abbrev};

        if (info.name.empty() || name.find_first_of(ascii_whitespace) != npos
                || std::find_if(name.begin(), name.end(), ascii_iscntrl) != name.end())
            throw std::invalid_argument("Invalid long option: {0:q}"_fmt(name));
        if (option_index(info.name) != npos)
            throw std::invalid_argument("Duplicate long option: {0:q}"_fmt(long_name));

        if (info.abbrev != '\0') {
            if (! ascii_isgraph(info.abbrev) || info.abbrev == '-')
                throw std::invalid_argument("Invalid short option: {0:q}"_fmt(short_name));
            if (option_index(info.abbrev) != npos)
                throw std::invalid_argument("Duplicate short option: {0:q}"_fmt(short_name));
        }

        if (info.kind == mode::boolean && info.is_anon)
            throw std::invalid_argument("Boolean options can't be anonymous: {0:q}"_fmt(long_name));
        if (info.kind == mode::boolean && info.is_required)
            throw std::invalid_argument("Boolean options can't be required: {0:q}"_fmt(long_name));
        if (info.is_anon) {
            if (anon_complete)
                throw std::invalid_argument("All anonymous arguments are already accounted for: {0:q}"_fmt(long_name));
            anon_complete = info.kind == mode::multiple;
        }

        if (info.description.empty())
            throw std::invalid_argument("Invalid option description: {0:q}"_fmt(description));

        options_.push_back(info);

    }

    std::string Options::format_help() const {

        auto xterm = colour_ == -1 ? Xterm() : Xterm(bool(colour_));
        auto head_colour = xterm.rgb(5, 5, 1); // bright yellow
        auto body_colour = xterm.rgb(5, 5, 3); // pale yellow
        auto prefix_colour = xterm.rgb(1, 5, 1); // green
        auto suffix_colour = xterm.rgb(2, 4, 5); // cyan

        std::string text = "\n"
            "{3}{4}{0}{1}{6}\n\n"
            "{5}{2}{6}\n\n"
            "{5}Options:{6}\n"_fmt
        (app_, version_, description_,
            xterm.bold(), head_colour, body_colour, xterm.reset());

        std::vector<std::string> left, right;
        std::string block;
        size_t left_width = 0;

        for (auto& info: options_) {

            block.clear();

            if (info.is_anon)
                block += '[';
            block += "--" + info.name;
            if (info.abbrev != '\0')
                block += ", -" + std::string{info.abbrev};
            if (info.is_anon)
                block += ']';

            if (info.kind != mode::boolean) {
                block += " " + info.placeholder;
                if (info.kind == mode::multiple)
                    block += " ...";
            }

            left.push_back(block);
            left_width = std::max(left_width, block.size());

            block = info.description;

            if (info.is_required || ! info.default_value.empty()) {
                if (block.back() == ')') {
                    block.pop_back();
                    block += "; ";
                } else {
                    block += " (";
                }
                if (info.is_required)
                    block += "required";
                else
                    block += "default " + info.default_value;
                block += ")";
            }

            right.push_back(block);

        }

        for (size_t i = 0; i < left.size(); ++i) {
            left[i].resize(left_width, ' ');
            text += "    {2}{0}  {3}= {1}{4}\n"_fmt
                (left[i], right[i], prefix_colour, suffix_colour, xterm.reset());
        }

        text += '\n';
        if (! extra_.empty())
            text += "{1}{0}{2}\n\n"_fmt(extra_, body_colour, xterm.reset());

        return text;

    }

    size_t Options::option_index(const std::string& name) const {
        auto key = trim_name(name);
        auto it = std::find_if(options_.begin(), options_.end(),
            [&key] (const option_info& opt) { return opt.name == key; });;
        return it == options_.end() ? npos : size_t(it - options_.begin());
    }

    size_t Options::option_index(char abbrev) const {
        if (abbrev == 0)
            return npos;
        auto it = std::find_if(options_.begin(), options_.end(),
            [abbrev] (const option_info& opt) { return opt.abbrev == abbrev; });;
        return it == options_.end() ? npos : size_t(it - options_.begin());
    }

}
