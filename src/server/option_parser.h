#ifndef OPTION_PARSER_H
#define OPTION_PARSER_H

#include <map>
#include <set>
#include <string>
#include <cerrno>
#include "utility/types.h"
#include "utility/assert_helper.h"

namespace sk {

class option_parser {
public:
    option_parser();
    ~option_parser();

    option_parser(const option_parser&) = delete;
    option_parser& operator=(const option_parser&) = delete;

    /**
     * @brief register_option
     * @param sopt: short option, 0 means it does not have a short option
     * @param lopt: long option, NULL means it does not have a long option
     * @param desc: description of this option
     * @param value_name: value name used for usage print
     * @param required: true if this option must be provided
     * @param value: the value we should store back after parsing
     * @return 0 if succeeded, error code otherwise
     */
    template<typename T>
    int register_option(char sopt, const char *lopt, const char *desc, const char *value_name, bool required, T *value) {
        assert_retval(desc, -1);
        assert_retval(value, -1);

        if (sopt == 0 && !lopt) {
            print_error("at least one option should be provided.");
            return -EINVAL;
        }

        // check if there is already a short option
        if (sopt != 0) {
            auto it = sopt2conf_.find(sopt);
            if (it != sopt2conf_.end()) {
                print_error("short option '%c' exists.", sopt);
                return -EEXIST;
            }
        }

        // check if there is already a long option
        if (lopt) {
            auto it = lopt2conf_.find(lopt);
            if (it != lopt2conf_.end()) {
                print_error("long option '%s' exists.", lopt);
                return -EEXIST;
            }
        }

        option_config *conf = new option_config();
        if (!conf) return -ENOMEM;

        conf->sopt = sopt;
        if (lopt) conf->lopt = lopt;
        conf->required = required;
        conf->desc = desc;
        if (value_name) conf->vname = value_name;
        conf->value = void_ptr(value);
        set_value_type(conf, value);

        if (conf->vtype == option_config::VALUE_TYPE_BOOL && value_name != NULL) {
            std::string msg("bool option ");
            if (conf->sopt != 0) {
                msg.append(1, conf->sopt);
                if (!conf->lopt.empty())
                    msg.append(1, '(').append(conf->lopt).append(1, ')');
            } else {
                msg.append(conf->lopt);
            }
            msg.append(" has a value name ").append(value_name);

            print_warning("%s", msg.c_str());
        }

        if (conf->vtype != option_config::VALUE_TYPE_BOOL && value_name == NULL) {
            std::string msg("non-bool option ");
            if (conf->sopt != 0) {
                msg.append(1, conf->sopt);
                if (!conf->lopt.empty())
                    msg.append(1, '(').append(conf->lopt).append(1, ')');
            } else {
                msg.append(conf->lopt);
            }
            msg.append(" does not have a value name.");

            print_warning("%s", msg.c_str());
        }

        conf_list_.insert(conf);
        if (sopt != 0) sopt2conf_[conf->sopt] = conf;
        if (lopt) lopt2conf_[conf->lopt] = conf;

        return 0;
    }

    /*
     * argc & argv should be the two parameters passed to main()
     */
    int parse(int argc, const char **argv, void(*help_handler)());

private:
    struct option_config {
        enum value_type {
            VALUE_TYPE_HELP,
            VALUE_TYPE_BOOL,
            VALUE_TYPE_S32,
            VALUE_TYPE_U32,
            VALUE_TYPE_S64,
            VALUE_TYPE_U64,
            VALUE_TYPE_STRING
        };

        char sopt;         // short option
        std::string lopt;  // long option
        std::string desc;  // option description
        std::string vname; // value name, used for usage print
        bool required;     // if this option must be provided
        value_type vtype;  // value type

        void *value;       // pointer to the potential value
    };

private:
    int parse_long(const std::string& arg);
    int parse_short(int argc, const char **argv, const std::string& arg, int curr, int& next);

    void set_value_type(option_config *conf, bool *);
    void set_value_type(option_config *conf, s32 *);
    void set_value_type(option_config *conf, u32 *);
    void set_value_type(option_config *conf, s64 *);
    void set_value_type(option_config *conf, u64 *);
    void set_value_type(option_config *conf, std::string *);

    int set_value(option_config *conf, const char *arg);

    void remove_option(option_config *conf);

    void print_warning(const char *format, ...);
    void print_error(const char *format, ...);
    void print_usage(const char *program);

private:
    std::map<char, option_config*> sopt2conf_;        // short option -> config
    std::map<std::string, option_config*> lopt2conf_; // long option -> config
    std::set<option_config*> conf_list_;
};

} // namespace sk

#endif // OPTION_PARSER_H
