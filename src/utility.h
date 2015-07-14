#ifndef UTILITY_H
#define UTILITY_H

namespace sk {

template<typename T1, typename T2>
struct pair {
    typedef T1 first_type;
    typedef T2 second_type;

    T1 first;
    T2 second;
};

template<typename T>
struct identity {
    typedef T result_type;

    result_type& operator()(T& t) const { return t; }
    const result_type& operator()(const T& t) const { return t; }
};

template<typename P>
struct select1st {
    typedef typename P::first_type result_type;

    result_type& operator()(P& p) const { return p.first; }
    const result_type& operator()(const P& p) const { return p.first; }
};

template<typename P>
struct select2nd {
    typedef typename P::second_type result_type;

    result_type& operator()(P& p) const { return p.second; }
    const result_type& operator()(const P& p) const { return p.second; }
};

} // namespace sk

#endif // UTILITY_H
