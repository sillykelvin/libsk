#include <time.h>
#include <execinfo.h>
#include <unistd.h>
#include <sstream>
#include "assert_helper.h"

static bool can_print_backtrace() {
    const int RESET_INTERVAL = 60;
    const int MAX_PRINT_NUM  = 300;

    static int last_reset_time = 0;
    static int total_print_num = 0;

    if (total_print_num < MAX_PRINT_NUM) {
        ++total_print_num;
        return true;
    }

    int now = (int) time(NULL);
    if (now > last_reset_time + RESET_INTERVAL) {
        total_print_num = 0;
        last_reset_time = now;
        return true;
    }

    return false;
}

namespace sk {

const char *stacktrace() {
    if (!can_print_backtrace())
        return "";

    void *array[20];

    size_t size = backtrace(array, 20);
    char **strings = backtrace_symbols(array, size);

    static char result[2048];

    std::stringstream ss;
    ss << "Obtained " << size << " stack frames.\n";

    do {
#ifdef NDEBUG
        for (size_t i = 0; i < size; ++i)
            ss << strings[i] << '\n';
        break;
#else
        char exe_name[256];
        ssize_t exe_len = readlink("/proc/self/exe", exe_name, sizeof(exe_name) - 1);
        if (exe_len == -1) {
            sk_error("cannot get executable file name.");
            for (size_t i = 0; i < size; ++i)
                ss << strings[i] << '\n';

            break;
        }

        /*
         * according to the man page of readlink, it will not append a null character,
         * so we should append it ourselves
         */
        exe_name[exe_len] = '\0';

        for (size_t i = 0; i < size; ++i) {
            std::string symbol(strings[i]);

            std::string func_name;
            std::string offset;
            size_t left_paren  = symbol.find('(');
            size_t right_paren = symbol.find(')', left_paren);
            if (left_paren + 1 != right_paren) {
                size_t plus = symbol.find('+', left_paren);
                if (plus != std::string::npos) {
                    func_name = symbol.substr(left_paren + 1, plus - left_paren - 1);
                    offset = symbol.substr(plus, right_paren - plus);
                }
            }

            size_t left_bracket = symbol.find('[');
            size_t right_bracket = symbol.find(']');
            std::string address(symbol.substr(left_bracket + 1, right_bracket - left_bracket - 1));

            char command[1024] = {0};
            char output[1024] = {0};

            if (!func_name.empty()) {
                snprintf(command, sizeof(command), "c++filt %s 2>&1", func_name.c_str());
                FILE *fp = popen(command, "r");
                if (!fp) {
                    sk_error("cannot run command %s", command);
                    ss << symbol << '\n';
                    continue;
                }

                while (fgets(output, sizeof(output), fp) != NULL);

                func_name = output;
                func_name.erase(std::remove(func_name.begin(), func_name.end(), '\n'), func_name.end());

                pclose(fp);
            }

            snprintf(command, sizeof(command), "addr2line %p -e %s 2>&1", array[i], exe_name);
            FILE *fp = popen(command, "r");
            if (!fp) {
                sk_error("cannot run command %s", command);
                ss << symbol << '\n';
                continue;
            }

            while (fgets(output, sizeof(output), fp) != NULL);

            pclose(fp);

            address.append(": ").append(output);
            address.erase(std::remove(address.begin(), address.end(), '\n'), address.end());

            ss << symbol.substr(0, left_paren) << '(';
            if (!func_name.empty())
                ss << func_name << ' ' << offset;
            ss << ") ["
               << address << ']' << '\n';
        }
#endif

    } while (0);

    snprintf(result, sizeof(result), "%s", ss.str().c_str());

    free(strings);

    return result;
}

} // namespace sk
