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
    option_parser() {}
    ~option_parser();

    option_parser(const option_parser&) = delete;
    option_parser& operator=(const option_parser&) = delete;

    /**
     * @brief register_option
     * @param sopt: short option, 0 means it does not have a short option
     * @param lopt: long option, NULL means it does not have a long option
     * @param desc: description of this option
     * @param required: true if this option must be provided
     * @param value: the value we should store back after parsing
     * @return 0 if succeeded, error code otherwise
     */
    template<typename T>
    int register_option(char sopt, const char *lopt, const char *desc, bool required, T *value) {
        assert_retval(desc, -1);
        assert_retval(value, -1);

        // check if there is already a short option
        if (sopt != 0) {
            auto it = sopt2conf_.find(sopt);
            if (it != sopt2conf_.end()) {
                error("short option '%c' exists.", sopt);
                return -EEXIST;
            }
        }

        // check if there is already a long option
        if (lopt) {
            auto it = lopt2conf_.find(lopt);
            if (it != lopt2conf_.end()) {
                error("long option '%s' exists.", lopt);
                return -EEXIST;
            }
        }

        option_config *conf = new option_config();
        if (!conf) return -ENOMEM;

        conf->sopt = sopt;
        if (lopt) conf->lopt = lopt;
        conf->required = required;
        conf->desc = desc;
        conf->value = void_ptr(value);
        determine_value_type(conf, value);

        conf_list_.insert(conf);
        if (sopt != 0) sopt2conf_[conf->sopt] = conf;
        if (lopt) lopt2conf_[conf->lopt] = conf;

        return 0;
    }

    /*
     * argc & argv should be the two parameters passed to main()
     */
    int parse(int argc, const char **argv);

private:
    struct option_config {
        enum value_type {
            VALUE_TYPE_BOOL = 1,
            VALUE_TYPE_S32,
            VALUE_TYPE_U32,
            VALUE_TYPE_S64,
            VALUE_TYPE_U64,
            VALUE_TYPE_STRING
        };

        char sopt;        // short option
        std::string lopt; // long option
        std::string desc; // option description
        bool required;    // if this option must be provided
        value_type vtype; // value type

        void *value;      // pointer to the potential value
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

    void warning(const char *format, ...);
    void error(const char *format, ...);

private:
    std::map<char, option_config*> sopt2conf_;        // short option -> config
    std::map<std::string, option_config*> lopt2conf_; // long option -> config
    std::set<option_config*> conf_list_;
};

} // namespace sk

#endif // OPTION_PARSER_H
