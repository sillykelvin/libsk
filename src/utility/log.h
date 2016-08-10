#ifndef LOG_H
#define LOG_H

#include <stdio.h>

// TODO: enhance the logging macros

#define ERR(fmt, ...) printf("[ERROR][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define INF(fmt, ...) printf("[INFO ][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define DBG(fmt, ...) printf("[DEBUG][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define sk_error(fmt, ...) printf("[ERROR][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define sk_warn(fmt, ...)  printf("[WARN ][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define sk_info(fmt, ...)  printf("[INFO ][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define sk_debug(fmt, ...) printf("[DEBUG][%s][%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)


#endif // LOG_H
