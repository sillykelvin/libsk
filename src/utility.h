#ifndef UTILITY_H
#define UTILITY_H

namespace sk {

/*
 * like std::pair, used for map/rbtree to represent value type
 */
template<typename T1, typename T2>
struct pair {
    typedef T1 first_type;
    typedef T2 second_type;

    T1 first;
    T2 second;
};


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

} // namespace sk

#endif // UTILITY_H
