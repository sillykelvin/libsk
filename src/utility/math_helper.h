#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include <type_traits>
#include "utility/types.h"
#include "utility/utility.h"

namespace sk {

// generate a random number in [min, max]
int random(int min, int max);

bool probability_hit(int prob, int prob_max);
bool percentage_hit(int percentage);

template<typename T, typename B>
inline typename sk::if_<std::is_const<B>::value,
                        typename std::add_const<T>::type*,
                        typename std::remove_const<T>::type*>::type
byte_offset(B *base, offset_t offset) {
    typedef typename std::remove_const<B>::type* mutable_b_pointer;
    typedef typename std::remove_const<T>::type* mutable_t_pointer;

    char *addr = char_ptr(const_cast<mutable_b_pointer>(base)) + offset;
    return static_cast<mutable_t_pointer>(void_ptr(addr));
}

} // namespace sk

#endif // MATH_HELPER_H
