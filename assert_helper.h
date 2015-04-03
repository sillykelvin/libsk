#ifndef ASSERT_HELPER_H
#define ASSERT_HELPER_H

#include <assert.h>

// TODO: enhance this macro
#define ASSERT(exp) assert(exp)

#define assert_retval(exp, val)                 \
    do {                                        \
        if (exp)                                \
            break;                              \
        ASSERT(exp);                            \
        return val;                             \
    } while (0)

#define assert_retnone(exp)                     \
    do {                                        \
        if (exp)                                \
            break;                              \
        ASSERT(exp);                            \
        return;                                 \
    } while (0)

#define assert_noeffect(exp)                    \
    do {                                        \
        if (exp)                                \
            break;                              \
        ASSERT(exp);                            \
    } while (0)

#endif // ASSERT_HELPER_H
