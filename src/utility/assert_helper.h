#ifndef ASSERT_HELPER_H
#define ASSERT_HELPER_H

// TODO: enhance this macro
#define ASSERT(exp) assert(exp)

#define assert_retval(exp, val)                 \
    do {                                        \
        if (exp) break;                         \
        ASSERT(exp);                            \
        return (val);                           \
    } while (0)

#define assert_retnone(exp)                     \
    do {                                        \
        if (exp) break;                         \
        ASSERT(exp);                            \
        return;                                 \
    } while (0)

#define assert_noeffect(exp)                    \
    do {                                        \
        if (exp) break;                         \
        ASSERT(exp);                            \
    } while (0)

#define assert_break(exp)                       \
    {                                           \
        if (!(exp)) {                           \
            ASSERT(exp);                        \
            break;                              \
        }                                       \
    }

#define assert_continue(exp)                    \
    {                                           \
        if (!(exp)) {                           \
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
