#ifndef SINGLETON_H
#define SINGLETON_H

#include <stdlib.h>

namespace sk {

/**
 * Usage:
 *
 * 1. use macro DECLARE_SINGLETON
 * 2. make the constructor private
 *
 * Example:
 *
 * class SomeClass {
 *     DECLARE_SINGLETON(SomeClass);
 *
 * private:
 *     SomeClass(...) { ... }
 *
 * public:
 *     ~SomeClass() { ... }
 *
 *     ...
 * };
 *
 */
template<typename T>
class singleton {
public:
    static T *get() {
        if(!instance) {
            instance = new T;
            atexit(destroy);
        }

        return (T *) instance;
    }

    virtual ~singleton() {}

private:
    static void destroy() {
        if (instance) {
            delete instance;
            instance = NULL;
        }
    }

private:
    singleton() {}
    singleton (const singleton&);
    singleton& operator=(const singleton&);

private:
    static volatile T *instance;
};

template<class T>
volatile T *singleton<T>::instance = NULL;

} // namespace sk

#define DECLARE_SINGLETON(T)                \
    public:                                 \
        static T *get() {                   \
            return sk::singleton<T>::get(); \
        }                                   \
    private:                                \
        T (const T&);                       \
        T& operator=(const T&);             \
        friend class sk::singleton<T>

#endif // SINGLETON_H
