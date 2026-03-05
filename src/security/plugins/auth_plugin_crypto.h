/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace scratchbird {
namespace security {
namespace plugins {
namespace crypto {

inline std::string hexEncode(const uint8_t* bytes, std::size_t length) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        out[2 * i] = kHex[(bytes[i] >> 4) & 0x0F];
        out[2 * i + 1] = kHex[bytes[i] & 0x0F];
    }
    return out;
}

inline bool hmacSha256Hex(std::string_view key,
                          std::string_view message,
                          std::string* out_hex) {
    if (!out_hex) {
        return false;
    }

    uint8_t digest[EVP_MAX_MD_SIZE]{};
    std::size_t digest_len = 0;
    bool ok = true;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) {
        return false;
    }

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    if (!ctx) {
        EVP_MAC_free(mac);
        return false;
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(
        OSSL_MAC_PARAM_DIGEST,
        const_cast<char*>("SHA256"),
        0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(ctx,
                     reinterpret_cast<const unsigned char*>(
                         key.empty() ? "" : key.data()),
                     key.size(),
                     params) != 1) {
        ok = false;
    }
    if (ok && EVP_MAC_update(ctx,
                             reinterpret_cast<const unsigned char*>(
                                 message.empty() ? "" : message.data()),
                             message.size()) != 1) {
        ok = false;
    }
    if (ok && EVP_MAC_final(ctx, digest, &digest_len, sizeof(digest)) != 1) {
        ok = false;
    }

    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
#else
    unsigned int digest_len_legacy = 0;
    HMAC_CTX* ctx = HMAC_CTX_new();
    if (!ctx) {
        return false;
    }

    if (HMAC_Init_ex(ctx,
                     key.empty() ? nullptr : key.data(),
                     static_cast<int>(key.size()),
                     EVP_sha256(),
                     nullptr) != 1) {
        ok = false;
    }
    if (ok && HMAC_Update(ctx,
                          reinterpret_cast<const unsigned char*>(
                              message.empty() ? "" : message.data()),
                          message.size()) != 1) {
        ok = false;
    }
    if (ok && HMAC_Final(ctx, digest, &digest_len_legacy) != 1) {
        ok = false;
    }

    HMAC_CTX_free(ctx);
    digest_len = static_cast<std::size_t>(digest_len_legacy);
#endif
    if (!ok) {
        return false;
    }

    *out_hex = hexEncode(digest, digest_len);
    return true;
}

}  // namespace crypto
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
