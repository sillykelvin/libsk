#include <stdarg.h>
#include <string.h>
#include "option_parser.h"
#include "utility/string_helper.h"

namespace sk {

option_parser::option_parser() {
    option_config *conf = new option_config();
    sk_assert(conf);

    conf->sopt = 'h';
    conf->lopt = "help";
    conf->required = false;
    conf->desc = "display this help";
    conf->vname = "";
    conf->value = NULL;
    conf->vtype = option_config::VALUE_TYPE_HELP;

    conf_list_.insert(conf);
    sopt2conf_[conf->sopt] = conf;
    lopt2conf_[conf->lopt] = conf;
}

option_parser::~option_parser() {
    sopt2conf_.clear();
    lopt2conf_.clear();

    for (auto it = conf_list_.begin(), end = conf_list_.end(); it != end; ++it)
        delete *it;

    conf_list_.clear();
}

int option_parser::parse(int argc, const char **argv, void(*help_handler)()) {
    // search if there is help flag in options first
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);

            if (help_handler) {
                help_handler();
                return 0;
            }

            // no help handler provided, just exit
            exit(0);
        }
    }

    // start from 1, as we consider argv[0] as the program name
    int i = 1;
    while (i < argc) {
        std::string arg(argv[i]);

        if (arg.length() < 2 || arg[0] != '-') {
            print_warning("invalid arg \"%s\".", arg.c_str());
            ++i;
            continue;
        }

        // long option
        if (arg[1] == '-') {
            int ret = parse_long(arg);
            if (ret != 0)
                return ret;

            ++i;
            continue;
        }

        // short option
        int next = i + 1;
        int ret = parse_short(argc, argv, arg, i, next);
        if (ret != 0)
            return ret;

        i = next;
    }

    for (auto it = conf_list_.begin(), end = conf_list_.end(); it != end; ++it) {
        option_config *conf = *it;
        if (conf->required) {
            print_error("missing required option %c(%s).", conf->sopt, conf->lopt.c_str());
            return -EINVAL;
        }
    }

    return 0;
}

void option_parser::print_usage(const char *program) {
    printf("usage: %s [<options>]...\n\n", program);

    const size_t max_width = 30;
    std::string help;
    help.reserve(1024);
    for (auto it = conf_list_.begin(), end = conf_list_.end(); it != end; ++it) {
        option_config *conf = *it;
        help.append(4, ' ');

        if (conf->sopt != 0)
            help.append(1, '-').append(1, conf->sopt);
        else
            help.append(2, ' ');

        if (!conf->lopt.empty()) {
            if (conf->sopt != 0)
                help.append(", ");
            else
                help.append(2, ' ');

            help.append("--").append(conf->lopt);
            auto len = 4 + conf->lopt.length();
            if (!conf->vname.empty()) {
                help.append(1, '=').append(conf->vname);
                len = len + 1 + conf->vname.length();
            }

            if (len < max_width)
                help.append(max_width - len, ' ');
        } else {
            help.append(max_width, ' ');
        }

        if (conf->required)
            help.append("<required>  ");
        else
            help.append("[optional]  ");

        help.append(conf->desc);
        help.append(1, '\n');

        printf("%s", help.c_str());
        help.clear();
    }
}

int option_parser::parse_long(const std::string& arg) {
    std::string lopt;

    auto eq_pos = arg.find('=');
    if (eq_pos == arg.npos) {
        lopt = arg.substr(2);
    } else {
        lopt = arg.substr(2, eq_pos - 2);
    }

    // 0. long option must have at least 2 letters
    if (lopt.length() < 2) {
        print_warning("invalid long option \"%s\".", arg.c_str());
        return 0;
    }

    // 1. option not registered, ignore
    auto it = lopt2conf_.find(lopt);
    if (it == lopt2conf_.end()) {
        print_warning("unknown option \"%s\".", lopt.c_str());
        return 0;
    }

    option_config *conf = it->second;

    // 3. value type is bool
    if (conf->vtype == option_config::VALUE_TYPE_BOOL) {
        // 3.1 value type is bool, but there is = sign in arg
        if (eq_pos != arg.npos) {
            print_error("invalid arg \"%s\".", arg.c_str());
            return -EINVAL;
        }

        bool *v = cast_ptr(bool, conf->value);
        *v = true;

        // remove the parsed option
        remove_option(conf);

        return 0;
    }

    // 4. value type is not bool, but there is no = sign in arg
    if (eq_pos == arg.npos) {
        print_error("option \"%s\" does not have a value.", lopt.c_str());
        return -EINVAL;
    }

    std::string value = arg.substr(eq_pos + 1);

    // 5. value is required, but it is empty
    if (value.empty()) {
        print_error("invalid arg \"%s\".", arg.c_str());
        return -EINVAL;
    }

    int ret = set_value(conf, value.c_str());
    if (ret != 0)
        return ret;

    // remove the parsed option
    remove_option(conf);

    return 0;
}

int option_parser::parse_short(int argc, const char **argv, const std::string& arg, int curr, int& next) {
    // set next to the next arg by default
    next = curr + 1;

    std::string::size_type i = 1; // arg[0] is '-', so we start from 1
    while (i < arg.length()) {
        char sopt = arg[i];
        auto it = sopt2conf_.find(sopt);

        // 0. option not registered, ignore
        if (it == sopt2conf_.end()) {
            print_warning("unknown option '%c'.", sopt);
            ++i;
            continue;
        }

        option_config *conf = it->second;

        // 1. value type is bool, set it to true directly
        if (conf->vtype == option_config::VALUE_TYPE_BOOL) {
            bool *v = cast_ptr(bool, conf->value);
            *v = true;

            // remove the parsed option
            remove_option(conf);

            ++i;
            continue;
        }

        // 2. value type is not bool, but it is not the last option
        if (i != arg.length() - 1) {
            print_error("invalid arg \"%s\".", arg.c_str());
            return -EINVAL;
        }

        // 3. value type is not bool and it is the last option, but there is no more arg
        if (curr + 1 >= argc) {
            print_error("option '%c' does not have a value.", sopt);
            return -EINVAL;
        }

        // 4. value type is not bool, it is the last option, and there is more arg
        int ret = set_value(conf, argv[curr + 1]);
        if (ret != 0)
            return ret;

        // remove the parsed option
        remove_option(conf);

        ++i;
        next = curr + 2;
    }

    return 0;
}

void option_parser::set_value_type(option_parser::option_config *conf, bool *) {
    conf->vtype = option_config::VALUE_TYPE_BOOL;
}

void option_parser::set_value_type(option_parser::option_config *conf, s32 *) {
    conf->vtype = option_config::VALUE_TYPE_S32;
}

void option_parser::set_value_type(option_parser::option_config *conf, u32 *) {
    conf->vtype = option_config::VALUE_TYPE_U32;
}

void option_parser::set_value_type(option_parser::option_config *conf, s64 *) {
    conf->vtype = option_config::VALUE_TYPE_S64;
}

void option_parser::set_value_type(option_parser::option_config *conf, u64 *) {
    conf->vtype = option_config::VALUE_TYPE_U64;
}

void option_parser::set_value_type(option_parser::option_config *conf, std::string *) {
    conf->vtype = option_config::VALUE_TYPE_STRING;
}

int option_parser::set_value(option_parser::option_config *conf, const char *arg) {
    int ret = 0;

    switch (conf->vtype) {
    case option_config::VALUE_TYPE_S32: {
        s32 *v = cast_ptr(s32, conf->value);
        ret = string2s32(arg, *v);
        break;
    }
    case option_config::VALUE_TYPE_U32: {
        u32 *v = cast_ptr(u32, conf->value);
        ret = string2u32(arg, *v);
        break;
    }
    case option_config::VALUE_TYPE_S64: {
        s64 *v = cast_ptr(s64, conf->value);
        ret = string2s64(arg, *v);
        break;
    }
    case option_config::VALUE_TYPE_U64: {
        u64 *v = cast_ptr(u64, conf->value);
        ret = string2u64(arg, *v);
        break;
    }
    case option_config::VALUE_TYPE_STRING: {
        std::string *v = cast_ptr(std::string, conf->value);
        *v = arg;
        ret = 0;
        break;
    }
    default: {
        sk_assert(0);
        ret = -EINVAL;
        break;
    }
    }

    return ret;
}

void option_parser::remove_option(option_parser::option_config *conf) {
    if (conf->sopt != 0) sopt2conf_.erase(conf->sopt);
    if (!conf->lopt.empty()) lopt2conf_.erase(conf->lopt);
    conf_list_.erase(conf);
    conf->value = NULL;
    delete conf;
}

void option_parser::print_warning(const char *format, ...) {
    printf("warning: ");

    va_list ap;
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);

    printf("\n");
}

void option_parser::print_error(const char *format, ...) {
    printf("error: ");

    va_list ap;
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);

    printf("\n");
}

} // namespace sk
