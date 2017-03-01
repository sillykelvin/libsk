#ifndef _TEST_CONFIG_H_
#define _TEST_CONFIG_H_

#include <vector>
#include <string>

#define DEF_CONFIG struct

namespace test {

namespace detail
{

DEF_CONFIG category {
    DEF_CONFIG device
    {
        std::string type;
        std::string pattern;
        int buffer_size;
        std::vector<u64> guid_list;
        std::vector<std::string> paths;
        std::vector<double> precisions;
    };

    DEF_CONFIG   filter    {
        std::string type;
        int count;

        std::vector<device> ref_devs;
        std::vector<float> averages;
    };

    std::string name; // test comment
    int level;
    u32 max_size;
    double precision;
    /*
     * multiline comments here
     */
    device main_dev;
    std::vector<device> backup_devs;
    std::vector<filter> filters;
};

      } // namespace detail

DEF_CONFIG log_conf    
{
    /* test comment again */
    int magic;
    int default_level;
    std::vector<category> categories;
};

} // namespace test

#endif // _TEST_CONFIG_H_
