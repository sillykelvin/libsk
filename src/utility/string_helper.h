#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include "utility/types.h"

namespace sk {

int string2s32(const char *str, s32& val);
int string2u32(const char *str, u32& val);
int string2s64(const char *str, s64& val);
int string2u64(const char *str, u64& val);
int string2float(const char *str, float& val);
int string2double(const char *str, double& val);

} // namespace sk

#endif // STRING_HELPER_H
