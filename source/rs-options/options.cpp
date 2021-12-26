#include "rs-options/options.hpp"
#include "rs-format/table.hpp"
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

    Options::Options(const std::string& app, const std::string& version, const std::string& description,
        const std::string& extra):
    options_(), app_(trim(app)), version_(trim(version)), description_(trim(description)),
    extra_(trim(extra)), auto_help_(false) {
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
        add(want_help, "help", help_abbrev, 0, "Show help");
        add(want_version, "version", version_abbrev, 0, "Show version");

        if (auto_help_ && args.empty()) {
            out << format_help();
            return false;
        }

        option_info* current = nullptr;
        size_t i = 0;
        bool escaped = false;

        auto found = [&current] (option_info& opt) {
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
                        throw std::invalid_argument("Argument not associated with an option: " + quote(arg));
                    found(*it);
                }

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
                        throw std::invalid_argument("Unknown option: " + quote(arg));
                    found(options_[j]);
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
                    throw std::invalid_argument("Unknown option: " + quote(arg));
                found(options_[j]);
                ++i;

            }

        }

        auto it = std::find_if(options_.begin(), options_.end(), [] (auto& opt) { return opt.is_required && ! opt.found; });
        if (it != options_.end())
            throw std::invalid_argument("Required option not found: " + quote("--" + it->name));

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
            int flags, const std::string& description, const std::string& placeholder,
            const std::string& default_value, mode kind) {

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

        auto quoted_name = [&info] { return quote("--" + info.name); };
        auto quoted_abbrev = [&info] { return quote({'-', info.abbrev}); };

        if (info.name.empty() || name.find_first_of(ascii_whitespace) != npos
                || std::find_if(name.begin(), name.end(), ascii_iscntrl) != name.end())
            throw std::invalid_argument("Invalid long option: " + quote(name));
        if (option_index(info.name) != npos)
            throw std::invalid_argument("Duplicate long option: " + quoted_name());

        if (info.abbrev != '\0') {
            if (! ascii_isgraph(info.abbrev) || info.abbrev == '-')
                throw std::invalid_argument("Invalid short option: " + quoted_abbrev());
            if (option_index(info.abbrev) != npos)
                throw std::invalid_argument("Duplicate short option: " + quoted_abbrev());
        }

        if (info.kind == mode::boolean && info.is_anon)
            throw std::invalid_argument("Boolean options can't be anonymous: " + quoted_name());
        if (info.kind == mode::boolean && info.is_required)
            throw std::invalid_argument("Boolean options can't be required: " + quoted_name());
        if (info.is_anon) {
            if (anon_complete)
                throw std::invalid_argument("All anonymous arguments are already accounted for: " + quoted_name());
            anon_complete = info.kind == mode::multiple;
        }

        if (info.description.empty())
            throw std::invalid_argument("Invalid option description: " + quote(description));

        options_.push_back(info);

    }

    std::string Options::format_help() const {

        std::string text = "\n{0}{1}\n\n{2}\n\nOptions:\n"_fmt(app_, version_, description_);
        Table tab;

        for (auto& info: options_) {

            std::string usage;

            if (info.is_anon)
                usage += '[';
            usage += "--" + info.name;
            if (info.abbrev != '\0')
                usage += ", -" + std::string{info.abbrev};
            if (info.is_anon)
                usage += ']';

            if (info.kind != mode::boolean) {
                usage += " " + info.placeholder;
                if (info.kind == mode::multiple)
                    usage += " ...";
            }

            std::string desc = info.description;

            if (info.is_required || ! info.default_value.empty()) {
                if (desc.back() == ')') {
                    desc.pop_back();
                    desc += "; ";
                } else {
                    desc += " (";
                }
                if (info.is_required)
                    desc += "required";
                else
                    desc += "default " + info.default_value;
                desc += ")";
            }

            tab.add(usage, desc);

        }

        text += indent_lines(to_string(tab));
        if (! extra_.empty())
            text = "{0}\n{1}\n"_fmt(text, extra_);
        text += '\n';

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
