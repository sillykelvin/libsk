#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <utility/crypto_helper.h>
#include <utility/assert_helper.h>
#include <utility/string_helper.h>

NS_BEGIN(sk)

static const char *basis64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char pr2six[256] = {
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

std::string base64_encode(const std::string& bin, bool url) {
    size_t i    = 0;
    u32 bytes   = 0;
    size_t bits = 0;
    size_t pos  = 0;
    size_t len  = bin.length();
    std::string output((((len + 2) / 3) * 4), '=');

    while (i < len) {
        u8 byte = cast_u8(bin[i++]);
        bytes = (bytes << 8) | (byte & 0xFF);
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            output[pos++] = basis64[(bytes >> bits) & 0x3F];
            if (url) {
                switch (output[pos]) {
                case '+': output[pos] = '-'; break;
                case '/': output[pos] = '_'; break;
                }
            }
        }
    }

    if (bits > 0) {
        sk_assert(bits < 6);
        bytes <<= 6 - bits;
        output[pos++] = basis64[bytes & 0x3F];
    }

    sk_assert(pos <= output.length() && pos >= output.length() - 2);
    return output;
}

std::string base64_decode(const std::string& str) {
    size_t i    = 0;
    u32 bytes   = 0;
    size_t bits = 0;
    size_t len  = str.length();
    std::string output;

    while (i < len) {
        u8 byte = cast_u8(str[i++]);
        switch (byte) {
        case '-': byte = '+'; break;
        case '_': byte = '/'; break;
        }

        byte = pr2six[byte];
        check_break(byte < 64);


        bytes = (bytes << 6) | byte;
        bits += 6;
        if (bits >= 8 ) {
            bits -= 8;
            output += cast_s8((bytes >> bits) & 0xFF);
        }
    }

    return output;
}

std::string md5_string(const std::string& str) {
    u8 md[16] = {0};
    MD5(reinterpret_cast<const u8*>(str.data()), str.length(), md);

    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (int i = 0; i < 16; ++i) ss << std::setw(2) << cast_int(md[i]);

    return ss.str();
}

bool verify_signature(const std::string& data, const std::string& key,
                      const std::string& name, const std::string& sig) {
    static const std::string public_key_prefix("-----BEGIN PUBLIC KEY-----");
    static const std::string pubrsa_key_prefix("-----BEGIN RSA PUBLIC KEY-----");
    static const std::string certificate_prefix("-----BEGIN CERTIFICATE-----");

    static bool openssl_inited = false;
    if (unlikely(!openssl_inited)) {
        OpenSSL_add_all_digests();
        openssl_inited = true;
    }

    bool ok = false;
    BIO *bp = nullptr;
    X509 *x509 = nullptr;
    EVP_PKEY *pkey = nullptr;
    const EVP_MD *md = nullptr;
    EVP_MD_CTX *mdctx = nullptr;

    do {
        md = EVP_get_digestbyname(name.c_str());
        check_break(md);

        bp = BIO_new_mem_buf(key.data(), key.length());
        check_break(bp);

        if (sk::is_prefix(public_key_prefix, key)) {
            pkey = PEM_read_bio_PUBKEY(bp, nullptr, nullptr, nullptr);
            check_break(pkey);
        } else if (sk::is_prefix(pubrsa_key_prefix, key)) {
            RSA *rsa = PEM_read_bio_RSAPublicKey(bp, nullptr, nullptr, nullptr);
            if (rsa) {
                pkey = EVP_PKEY_new();
                if (pkey) EVP_PKEY_set1_RSA(pkey, rsa);
                RSA_free(rsa);
            }
            check_break(pkey);
        } else if (sk::is_prefix(certificate_prefix, key)) {
            x509 = PEM_read_bio_X509(bp, nullptr, nullptr, nullptr);
            check_break(x509);

            pkey = X509_get_pubkey(x509);
            check_break(pkey);
        } else {
            assert_break(0);
        }

        mdctx = EVP_MD_CTX_create();
        check_break(mdctx);

        check_break(EVP_DigestVerifyInit(mdctx, nullptr, md, nullptr, pkey) == 1);
        check_break(EVP_DigestVerifyUpdate(mdctx, data.data(), data.length()) == 1);

        ok = (EVP_DigestVerifyFinal(mdctx, reinterpret_cast<const u8*>(sig.data()), sig.length()) == 1);
    } while (0);

    if (pkey)  EVP_PKEY_free(pkey);
    if (x509)  X509_free(x509);
    if (bp)    BIO_free_all(bp);
    if (mdctx) EVP_MD_CTX_destroy(mdctx);

    return ok;
}

NS_END(sk)
