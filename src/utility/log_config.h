#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

#include <string>
#include <vector>
#include "utility/types.h"

#define DEF_CONFIG struct

namespace sk {

DEF_CONFIG log_config {
    DEF_CONFIG file_device {
        std::string path; // the file path
        u32 max_size;     // the max size of a single file
        u32 max_rotation; // the max rotation of a specified log file
    };

    DEF_CONFIG net_device {
        std::string address; // ip address of the log server
        u16 port;            // port of the log server
    };

    DEF_CONFIG category {
        std::string name;    // logger name
        std::string pattern; // log pattern
        std::string level;   // log level

        std::vector<file_device> fdev_list;
        std::vector<net_device>  ndev_list;
    };

    std::vector<category> categories;
};

} // namespace sk

#endif // LOG_CONFIG_H
