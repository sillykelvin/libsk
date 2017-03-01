#ifndef ASSERT_HELPER_H
#define ASSERT_HELPER_H

#include <assert.h>
#include "log/log.h"
#include "utility/types.h"

namespace sk {
const char *stacktrace();
} // namespace sk

#define ASSERT(exp)                                    \
    do {                                               \
        sk_fatal("assert failure: %s", #exp);          \
        sk_fatal("stack trace: %s", sk::stacktrace()); \
        assert(exp);                                   \
    } while (0)

#define sk_assert(exp)                          \
    do {                                        \
        if (likely(exp)) break;                 \
        ASSERT(exp);                            \
    } while (0)

#define assert_retval(exp, val)                 \
    do {                                        \
        if (likely(exp)) break;                 \
        ASSERT(exp);                            \
        return (val);                           \
    } while (0)

#define assert_retnone(exp)                     \
    do {                                        \
        if (likely(exp)) break;                 \
        ASSERT(exp);                            \
        return;                                 \
    } while (0)

#define assert_break(exp)                       \
    {                                           \
        if (unlikely(!(exp))) {                 \
            ASSERT(exp);                        \
            break;                              \
        }                                       \
    }

#define assert_continue(exp)                    \
    {                                           \
        if (unlikely(!(exp))) {                 \
            ASSERT(exp);                        \
            continue;                           \
        }                                       \
    }


#define check_retval(exp, val)                  \
    do {                                        \
        if (exp) break;                         \
        return (val);                           \
    } while (0)

#define check_retnone(exp)                      \
    do {                                        \
        if (exp) break;                         \
        return;                                 \
    } while (0)

#define check_break(exp)                        \
    if (!(exp)) break                           \

#define check_continue(exp)                     \
    if (!(exp)) continue                        \

#endif // ASSERT_HELPER_H
