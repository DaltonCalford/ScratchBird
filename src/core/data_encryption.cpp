#include "scratchbird/core/data_encryption.h"
#include <random>

#ifdef __has_include
#if __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#include <openssl/rand.h>
#define HAVE_OPENSSL_CRYPTO 1
#endif
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace scratchbird::core
{
    namespace
    {
        constexpr size_t kGcmIvSize = 12;
        constexpr size_t kGcmTagSize = 16;

        Status generateRandomBytes(std::vector<uint8_t> &out, size_t len, ErrorContext *ctx)
        {
            out.assign(len, 0);
#ifdef HAVE_OPENSSL_CRYPTO
            if (RAND_bytes(out.data(), static_cast<int>(len)) != 1)
            {
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Failed to generate secure random bytes");
                return Status::INTERNAL_ERROR;
            }
            return Status::OK;
#else
            std::random_device rd;
            for (size_t i = 0; i < len; ++i)
            {
                out[i] = static_cast<uint8_t>(rd());
            }
            return Status::OK;
#endif
        }

#ifdef HAVE_OPENSSL_CRYPTO
        Status encryptGcm(const std::vector<uint8_t> &plaintext,
                          const std::vector<uint8_t> &key,
                          const EVP_CIPHER *cipher,
                          EncryptedValue &encrypted_out,
                          ErrorContext *ctx)
        {
            Status status = generateRandomBytes(encrypted_out.iv, kGcmIvSize, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            EVP_CIPHER_CTX *ctx_raw = EVP_CIPHER_CTX_new();
            if (!ctx_raw)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate cipher context");
                return Status::OOM;
            }

            encrypted_out.ciphertext.assign(plaintext.size(), 0);
            encrypted_out.auth_tag.assign(kGcmTagSize, 0);

            int len = 0;
            int ciphertext_len = 0;
            if (EVP_EncryptInit_ex(ctx_raw, cipher, nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_SET_IVLEN,
                                    static_cast<int>(encrypted_out.iv.size()), nullptr) != 1 ||
                EVP_EncryptInit_ex(ctx_raw, nullptr, nullptr, key.data(), encrypted_out.iv.data()) != 1 ||
                EVP_EncryptUpdate(ctx_raw, encrypted_out.ciphertext.data(), &len,
                                  plaintext.data(), static_cast<int>(plaintext.size())) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM encryption failed");
                return Status::INTERNAL_ERROR;
            }

            ciphertext_len = len;
            if (EVP_EncryptFinal_ex(ctx_raw, encrypted_out.ciphertext.data() + len, &len) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM finalization failed");
                return Status::INTERNAL_ERROR;
            }
            ciphertext_len += len;
            encrypted_out.ciphertext.resize(static_cast<size_t>(ciphertext_len));

            if (EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_GET_TAG,
                                    static_cast<int>(encrypted_out.auth_tag.size()),
                                    encrypted_out.auth_tag.data()) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM tag extraction failed");
                return Status::INTERNAL_ERROR;
            }

            EVP_CIPHER_CTX_free(ctx_raw);
            return Status::OK;
        }

        Status decryptGcm(const EncryptedValue &encrypted,
                          const std::vector<uint8_t> &key,
                          const EVP_CIPHER *cipher,
                          std::vector<uint8_t> &plaintext_out,
                          ErrorContext *ctx)
        {
            if (encrypted.iv.empty() || encrypted.auth_tag.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing IV or authentication tag");
                return Status::INVALID_ARGUMENT;
            }

            EVP_CIPHER_CTX *ctx_raw = EVP_CIPHER_CTX_new();
            if (!ctx_raw)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate cipher context");
                return Status::OOM;
            }

            plaintext_out.assign(encrypted.ciphertext.size(), 0);

            int len = 0;
            int plaintext_len = 0;
            if (EVP_DecryptInit_ex(ctx_raw, cipher, nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_SET_IVLEN,
                                    static_cast<int>(encrypted.iv.size()), nullptr) != 1 ||
                EVP_DecryptInit_ex(ctx_raw, nullptr, nullptr, key.data(), encrypted.iv.data()) != 1 ||
                EVP_DecryptUpdate(ctx_raw, plaintext_out.data(), &len,
                                  encrypted.ciphertext.data(),
                                  static_cast<int>(encrypted.ciphertext.size())) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM decryption failed");
                return Status::INTERNAL_ERROR;
            }

            plaintext_len = len;
            if (EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_SET_TAG,
                                    static_cast<int>(encrypted.auth_tag.size()),
                                    const_cast<uint8_t *>(encrypted.auth_tag.data())) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM tag setup failed");
                return Status::INTERNAL_ERROR;
            }

            int final_status = EVP_DecryptFinal_ex(ctx_raw, plaintext_out.data() + len, &len);
            EVP_CIPHER_CTX_free(ctx_raw);
            if (final_status <= 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "AES-GCM authentication failed");
                return Status::DATA_CORRUPTED;
            }

            plaintext_len += len;
            plaintext_out.resize(static_cast<size_t>(plaintext_len));
            return Status::OK;
        }
#endif
    } // namespace

    Status DataEncryption::encrypt(const std::vector<uint8_t> &plaintext,
                                   const std::vector<uint8_t> &key,
                                   EncryptionAlgorithm algorithm,
                                   EncryptedValue &encrypted_out,
                                   ErrorContext *ctx)
    {
        encrypted_out.ciphertext.clear();
        encrypted_out.iv.clear();
        encrypted_out.auth_tag.clear();
        encrypted_out.algorithm = algorithm;

        switch (algorithm)
        {
        case EncryptionAlgorithm::AES256_GCM:
            return encryptAES256GCM(plaintext, key, encrypted_out, ctx);
        case EncryptionAlgorithm::AES128_GCM:
            return encryptAES128GCM(plaintext, key, encrypted_out, ctx);
        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported encryption algorithm");
            return Status::INVALID_ARGUMENT;
        }
    }

    Status DataEncryption::decrypt(const EncryptedValue &encrypted,
                                   const std::vector<uint8_t> &key,
                                   std::vector<uint8_t> &plaintext_out,
                                   ErrorContext *ctx)
    {
        switch (encrypted.algorithm)
        {
        case EncryptionAlgorithm::AES256_GCM:
            return decryptAES256GCM(encrypted, key, plaintext_out, ctx);
        case EncryptionAlgorithm::AES128_GCM:
            return decryptAES128GCM(encrypted, key, plaintext_out, ctx);
        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported encryption algorithm");
            return Status::INVALID_ARGUMENT;
        }
    }

    void DataEncryption::generateIV(std::vector<uint8_t> &iv_out)
    {
        ErrorContext ctx;
        Status status = generateRandomBytes(iv_out, kGcmIvSize, &ctx);
        if (status != Status::OK)
        {
            iv_out.clear();
        }
    }

    bool DataEncryption::hasHardwareAcceleration()
    {
#if defined(__x86_64__) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_cpu_supports("aes");
#elif defined(_MSC_VER)
        int cpu_info[4] = {0, 0, 0, 0};
        __cpuid(cpu_info, 1);
        return (cpu_info[2] & (1 << 25)) != 0;
#else
        return false;
#endif
#else
        return false;
#endif
    }

    Status DataEncryption::encryptAES256GCM(const std::vector<uint8_t> &plaintext,
                                            const std::vector<uint8_t> &key,
                                            EncryptedValue &encrypted_out,
                                            ErrorContext *ctx)
    {
        if (key.size() != 32)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key length for AES-256-GCM");
            return Status::INVALID_ARGUMENT;
        }
#ifdef HAVE_OPENSSL_CRYPTO
        return encryptGcm(plaintext, key, EVP_aes_256_gcm(), encrypted_out, ctx);
#else
        (void)plaintext;
        (void)encrypted_out;
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for AES-GCM");
        return Status::NOT_SUPPORTED;
#endif
    }

    Status DataEncryption::encryptAES128GCM(const std::vector<uint8_t> &plaintext,
                                            const std::vector<uint8_t> &key,
                                            EncryptedValue &encrypted_out,
                                            ErrorContext *ctx)
    {
        if (key.size() != 16)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key length for AES-128-GCM");
            return Status::INVALID_ARGUMENT;
        }
#ifdef HAVE_OPENSSL_CRYPTO
        return encryptGcm(plaintext, key, EVP_aes_128_gcm(), encrypted_out, ctx);
#else
        (void)plaintext;
        (void)encrypted_out;
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for AES-GCM");
        return Status::NOT_SUPPORTED;
#endif
    }

    Status DataEncryption::decryptAES256GCM(const EncryptedValue &encrypted,
                                            const std::vector<uint8_t> &key,
                                            std::vector<uint8_t> &plaintext_out,
                                            ErrorContext *ctx)
    {
        if (key.size() != 32)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key length for AES-256-GCM");
            return Status::INVALID_ARGUMENT;
        }
#ifdef HAVE_OPENSSL_CRYPTO
        return decryptGcm(encrypted, key, EVP_aes_256_gcm(), plaintext_out, ctx);
#else
        (void)encrypted;
        (void)plaintext_out;
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for AES-GCM");
        return Status::NOT_SUPPORTED;
#endif
    }

    Status DataEncryption::decryptAES128GCM(const EncryptedValue &encrypted,
                                            const std::vector<uint8_t> &key,
                                            std::vector<uint8_t> &plaintext_out,
                                            ErrorContext *ctx)
    {
        if (key.size() != 16)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key length for AES-128-GCM");
            return Status::INVALID_ARGUMENT;
        }
#ifdef HAVE_OPENSSL_CRYPTO
        return decryptGcm(encrypted, key, EVP_aes_128_gcm(), plaintext_out, ctx);
#else
        (void)encrypted;
        (void)plaintext_out;
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for AES-GCM");
        return Status::NOT_SUPPORTED;
#endif
    }
} // namespace scratchbird::core
