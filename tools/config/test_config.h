#ifndef _TEST_CONFIG_H_
#define _TEST_CONFIG_H_

#include <vector>
#include <string>

#define DEF_CONFIG struct


DEF_CONFIG category {
    DEF_CONFIG device
    {
        std::string type;
        std::string pattern;
        int buffer_size;
    };

    std::string name; // test comment
    int level;
    int max_size;
    /*
     * multiline comments here
     */
    device dev;
};

DEF_CONFIG log_conf    
{
    /* test comment again */
    int magic;
    int default_level;
    std::vector<category> categories;
};

#endif // _TEST_CONFIG_H_
