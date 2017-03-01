#ifndef BUS_CONFIG_H
#define BUS_CONFIG_H

#include <string>
#include <vector>
#include "utility/types.h"

#define DEF_CONFIG struct

DEF_CONFIG bus_config {
    int bus_shm_key;        // where channel_mgr to be stored
    int msg_per_run;        // message count to process in one loop
    int listen_port;        // which port to listen for incoming messages
    size_t shm_size;        // size of the shm segment used by the process itself
    size_t bus_shm_size;    // size of the shm segment used by bus channels
    size_t report_interval; // how many runs between two channel reports
    std::vector<std::string> consul_addr_list;

    int load_from_xml_file(const char *filename);
};

#endif // BUS_CONFIG_H
