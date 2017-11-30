#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include <type_traits>
#include <sys/types.h>
#include <utility/types.h>
#include <utility/utility.h>

namespace sk {

// generate a random number in [min, max]
int random(int min, int max);

bool probability_hit(int prob, int prob_max);
bool percentage_hit(int percentage);

void memrev16(void *p);
void memrev32(void *p);
void memrev64(void *p);
s16  intrev16(s16 v);
u16  intrev16(u16 v);
s32  intrev32(s32 v);
u32  intrev32(u32 v);
s64  intrev64(s64 v);
u64  intrev64(u64 v);

#if (BYTE_ORDER == BIG_ENDIAN)
#define htons16(v) (v)
#define ntohs16(v) (v)
#define htonu16(v) (v)
#define ntohu16(v) (v)
#define htons32(v) (v)
#define ntohs32(v) (v)
#define htonu32(v) (v)
#define ntohu32(v) (v)
#define htons64(v) (v)
#define ntohs64(v) (v)
#define htonu64(v) (v)
#define ntohu64(v) (v)
#else
#define htons16(v) sk::intrev16(v)
#define ntohs16(v) sk::intrev16(v)
#define htonu16(v) sk::intrev16(v)
#define ntohu16(v) sk::intrev16(v)
#define htons32(v) sk::intrev32(v)
#define ntohs32(v) sk::intrev32(v)
#define htonu32(v) sk::intrev32(v)
#define ntohu32(v) sk::intrev32(v)
#define htons64(v) sk::intrev64(v)
#define ntohs64(v) sk::intrev64(v)
#define htonu64(v) sk::intrev64(v)
#define ntohu64(v) sk::intrev64(v)
#endif

u16 crc16(const void *buf, size_t len);

template<typename T, typename B>
inline typename sk::if_<std::is_const<B>::value,
                        typename std::add_const<T>::type*,
                        typename std::remove_const<T>::type*>::type
byte_offset(B *base, size_t offset) {
    typedef typename std::remove_const<B>::type* mutable_b_pointer;
    typedef typename std::remove_const<T>::type* mutable_t_pointer;

    char *addr = char_ptr(const_cast<mutable_b_pointer>(base)) + offset;
    return static_cast<mutable_t_pointer>(void_ptr(addr));
}

} // namespace sk

#endif // MATH_HELPER_H
