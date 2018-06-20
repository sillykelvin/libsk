#ifndef CRYPTO_HELPER_H
#define CRYPTO_HELPER_H

#include <string>
#include <utility/types.h>

NS_BEGIN(sk)

std::string base64_encode(const std::string& bin, bool url);
std::string base64_decode(const std::string& str);

std::string md5_string(const std::string& str);

bool verify_signature(const std::string& data, const std::string& key,
                      const std::string& name, const std::string& sig);

NS_END(sk)

#endif // CRYPTO_HELPER_H
