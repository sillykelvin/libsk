#ifndef UTILITY_H
#define UTILITY_H

#include <utility>
#include <type_traits>
#include <utility/types.h>

NS_BEGIN(sk)

/*
 * type selector according to a bool value
 */
template<bool B, typename T, typename F>
struct if_;

template<typename T, typename F>
struct if_<true, T, F> {
    typedef T type;
};

template<typename T, typename F>
struct if_<false, T, F> {
    typedef F type;
};

/*
 * define a type according to a bool value
 */
template<bool, typename T = void>
struct enable_if;

template<typename T>
struct enable_if<true, T> {
    typedef T type;
};

/*
 * define the logical "and" operator
 */
template<typename...>
struct and_;

template<>
struct and_<> : std::true_type {};

template<typename T>
struct and_<T> : T {};

template<typename T, typename U>
struct and_<T, U> : if_<T::value, U, T>::type {};

template<typename T, typename U, typename V, typename... W>
struct and_<T, U, V, W...> : if_<T::value, and_<U, V, W...>, T>::type {};

/*
 * define the logical "or" operator
 */
template<typename...>
struct or_;

template<>
struct or_<> : std::false_type {};

template<typename T>
struct or_<T> : T {};

template<typename T, typename U>
struct or_<T, U> : if_<T::value, T, U>::type {};

template<typename T, typename U, typename V, typename... W>
struct or_<T, U, V, W...> : if_<T::value, T, or_<U, V, W...>>::type {};

/*
 * define the logical "not" operator
 */
template<typename T>
struct not_ : if_<T::value, std::false_type, std::true_type>::type {};

template<typename... Ts>
struct largest;

template<typename T>
struct largest<T> { typedef T type; };

template<typename T, typename U, typename... Ts>
struct largest<T, U, Ts...> {
    typedef typename largest<
        typename std::conditional<sizeof(U) <= sizeof(T), T, U>::type,
        Ts...>::type type;
};

template<typename T>
const T& max(const T& a, const T& b) {
    return a < b ? b : a;
}

template<typename T>
const T& min(const T& a, const T& b) {
    return a < b ? a : b;
}

/*
 * like std::pair, used for map/rbtree to represent value type
 */
template<typename T1, typename T2>
struct pair {
    typedef T1 first_type;
    typedef T2 second_type;

    T1 first;
    T2 second;

    pair() : first(), second() {}
    pair(const T1& first, const T2& second) : first(first), second(second) {}

    template<typename U1, typename U2, typename =
             typename enable_if<and_<std::is_convertible<U1, T1>,
                                     std::is_convertible<U2, T2>>::value>::type>
    pair(pair<U1, U2>&& p)
        : first(std::forward<U1>(p.first)), second(std::forward<U2>(p.second)) {}
};

/*
 * a convenient way to generate a pair with auto type deduce
 */
template<typename T1, typename T2>
pair<T1, T2> make_pair(T1&& t1, T2&& t2) {
    return pair<T1, T2>(std::forward<T1>(t1), std::forward<T2>(t2));
}

/*
 * single type selector, used for set/rbtree to extract key from value
 */
template<typename T>
struct identity {
    typedef T result_type;

    result_type& operator()(T& t) const { return t; }
    const result_type& operator()(const T& t) const { return t; }
};

/*
 * double types selector, select first type, used for map/rbtree to
 * extract key from key-value pair, P should be a sk::pair
 */
template<typename P>
struct select1st {
    typedef typename P::first_type result_type;

    result_type& operator()(P& p) const { return p.first; }
    const result_type& operator()(const P& p) const { return p.first; }
};

/*
 * double types selector, select second type
 */
template<typename P>
struct select2nd {
    typedef typename P::second_type result_type;

    result_type& operator()(P& p) const { return p.second; }
    const result_type& operator()(const P& p) const { return p.second; }
};

/*
 * choose a bigger number between N1 and N2
 */
template<size_t N1, size_t N2>
struct bigger {
    static const size_t value = N1 > N2 ? N1 : N2;
};

/*
 * choose a smaller number between N1 and N2
 */
template<size_t N1, size_t N2>
struct smaller {
    static const size_t value = N1 < N2 ? N1 : N2;
};

NS_END(sk)

#endif // UTILITY_H
