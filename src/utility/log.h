#ifndef LOG_H
#define LOG_H

// TODO: enhance the logging macros
// fix compilation error if gcc version >= 4.7
#if GCC_VERSION > 40700
    #define ERR(fmt, ...) printf("[ERROR][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
    #define DBG(fmt, ...) printf("[DEBUG][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define ERR(fmt, ...) printf("[ERROR][%s][%d] "fmt"\n", __FILE__, __LINE__, ##__VA_ARGS__)
    #define DBG(fmt, ...) printf("[DEBUG][%s][%d] "fmt"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#endif // LOG_H
