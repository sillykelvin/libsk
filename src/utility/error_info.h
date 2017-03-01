#ifndef ERROR_INFO_H
#define ERROR_INFO_H

#include <string>

namespace sk {

void set_last_error(int code, const char *fmt, ...);
void set_last_error(int code, const std::string& info);

int last_error_code();
const std::string& last_error_info();

} // namespace sk

#endif // ERROR_INFO_H
