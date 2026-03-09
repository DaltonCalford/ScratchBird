/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/data_encryption.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/buffer_pool.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>

#ifdef __has_include
#if __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#include <openssl/rand.h>
#define HAVE_OPENSSL_CRYPTO 1
#endif
#endif

namespace scratchbird::core
{
    namespace
    {
        constexpr uint32_t kDefaultKdfIterations = 10000;
        constexpr size_t kSaltSize = 16;
        constexpr size_t kIvSize = 12;
        constexpr size_t kTagSize = 16;
        constexpr uint8_t kWrappedKeyVersion = 1;

        void appendUint8(std::string &out, uint8_t value)
        {
            out.push_back(static_cast<char>(value));
        }

        void appendUint32(std::string &out, uint32_t value)
        {
            char buf[4];
            buf[0] = static_cast<char>(value & 0xFF);
            buf[1] = static_cast<char>((value >> 8) & 0xFF);
            buf[2] = static_cast<char>((value >> 16) & 0xFF);
            buf[3] = static_cast<char>((value >> 24) & 0xFF);
            out.append(buf, 4);
        }

        void appendUint16(std::string &out, uint16_t value)
        {
            char buf[2];
            buf[0] = static_cast<char>(value & 0xFF);
            buf[1] = static_cast<char>((value >> 8) & 0xFF);
            out.append(buf, 2);
        }

        bool readUint8(const std::string &blob, size_t &offset, uint8_t &value)
        {
            if (offset + 1 > blob.size())
            {
                return false;
            }
            value = static_cast<uint8_t>(blob[offset]);
            offset += 1;
            return true;
        }

        bool readUint32(const std::string &blob, size_t &offset, uint32_t &value)
        {
            if (offset + 4 > blob.size())
            {
                return false;
            }
            const uint8_t *data = reinterpret_cast<const uint8_t *>(blob.data() + offset);
            value = static_cast<uint32_t>(data[0]) |
                    (static_cast<uint32_t>(data[1]) << 8) |
                    (static_cast<uint32_t>(data[2]) << 16) |
                    (static_cast<uint32_t>(data[3]) << 24);
            offset += 4;
            return true;
        }

        bool readUint16(const std::string &blob, size_t &offset, uint16_t &value)
        {
            if (offset + 2 > blob.size())
            {
                return false;
            }
            const uint8_t *data = reinterpret_cast<const uint8_t *>(blob.data() + offset);
            value = static_cast<uint16_t>(data[0]) |
                    (static_cast<uint16_t>(data[1]) << 8);
            offset += 2;
            return true;
        }

        Status serializeEncryptedValue(const EncryptedValue &encrypted,
                                       std::vector<uint8_t> &out,
                                       ErrorContext *ctx)
        {
            if (encrypted.iv.size() > std::numeric_limits<uint16_t>::max() ||
                encrypted.auth_tag.size() > std::numeric_limits<uint16_t>::max() ||
                encrypted.ciphertext.size() > std::numeric_limits<uint32_t>::max())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Encrypted payload too large");
                return Status::INVALID_ARGUMENT;
            }

            std::string blob;
            appendUint8(blob, static_cast<uint8_t>(encrypted.algorithm));
            appendUint16(blob, static_cast<uint16_t>(encrypted.iv.size()));
            appendUint16(blob, static_cast<uint16_t>(encrypted.auth_tag.size()));
            appendUint32(blob, static_cast<uint32_t>(encrypted.ciphertext.size()));
            blob.append(reinterpret_cast<const char*>(encrypted.iv.data()),
                        encrypted.iv.size());
            blob.append(reinterpret_cast<const char*>(encrypted.auth_tag.data()),
                        encrypted.auth_tag.size());
            blob.append(reinterpret_cast<const char*>(encrypted.ciphertext.data()),
                        encrypted.ciphertext.size());

            out.assign(blob.begin(), blob.end());
            return Status::OK;
        }

        Status parseEncryptedValue(const std::vector<uint8_t> &blob,
                                   EncryptedValue &encrypted,
                                   ErrorContext *ctx)
        {
            std::string view(reinterpret_cast<const char*>(blob.data()), blob.size());
            size_t offset = 0;
            uint8_t algo = 0;
            uint16_t iv_len = 0;
            uint16_t tag_len = 0;
            uint32_t cipher_len = 0;

            if (!readUint8(view, offset, algo) ||
                !readUint16(view, offset, iv_len) ||
                !readUint16(view, offset, tag_len) ||
                !readUint32(view, offset, cipher_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid encrypted blob");
                return Status::DATA_CORRUPTED;
            }

            if (offset + iv_len + tag_len + cipher_len > view.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid encrypted blob length");
                return Status::DATA_CORRUPTED;
            }

            encrypted.algorithm = static_cast<EncryptionAlgorithm>(algo);
            encrypted.iv.assign(blob.begin() + offset, blob.begin() + offset + iv_len);
            offset += iv_len;
            encrypted.auth_tag.assign(blob.begin() + offset, blob.begin() + offset + tag_len);
            offset += tag_len;
            encrypted.ciphertext.assign(blob.begin() + offset, blob.begin() + offset + cipher_len);
            return Status::OK;
        }

        Status generateRandomBytes(std::vector<uint8_t> &out, size_t len, ErrorContext *ctx)
        {
            out.resize(len);
#ifdef HAVE_OPENSSL_CRYPTO
            if (RAND_bytes(out.data(), static_cast<int>(len)) != 1)
            {
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Failed to generate secure random bytes");
                return Status::INTERNAL_ERROR;
            }
#else
            std::random_device rd;
            for (size_t i = 0; i < len; ++i)
            {
                out[i] = static_cast<uint8_t>(rd());
            }
#endif
            return Status::OK;
        }

        Status deriveKeyPBKDF2(const std::vector<uint8_t> &input,
                               const std::vector<uint8_t> &salt,
                               uint32_t iterations,
                               size_t out_len,
                               std::vector<uint8_t> &key_out,
                               ErrorContext *ctx)
        {
#ifdef HAVE_OPENSSL_CRYPTO
            key_out.assign(out_len, 0);
            if (PKCS5_PBKDF2_HMAC(
                    reinterpret_cast<const char *>(input.data()),
                    static_cast<int>(input.size()),
                    salt.data(),
                    static_cast<int>(salt.size()),
                    static_cast<int>(iterations),
                    EVP_sha256(),
                    static_cast<int>(out_len),
                    key_out.data()) != 1)
            {
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "PBKDF2 key derivation failed");
                return Status::INTERNAL_ERROR;
            }
            return Status::OK;
#else
            (void)input;
            (void)salt;
            (void)iterations;
            (void)out_len;
            key_out.clear();
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for PBKDF2");
            return Status::NOT_SUPPORTED;
#endif
        }

        const EVP_CIPHER *selectCipher(size_t key_len)
        {
#ifdef HAVE_OPENSSL_CRYPTO
            if (key_len == 16)
            {
                return EVP_aes_128_gcm();
            }
            if (key_len == 32)
            {
                return EVP_aes_256_gcm();
            }
#else
            (void)key_len;
#endif
            return nullptr;
        }

        Status encryptAesGcm(const std::vector<uint8_t> &key,
                             const std::vector<uint8_t> &plaintext,
                             std::vector<uint8_t> &iv_out,
                             std::vector<uint8_t> &tag_out,
                             std::vector<uint8_t> &ciphertext_out,
                             ErrorContext *ctx)
        {
#ifdef HAVE_OPENSSL_CRYPTO
            const EVP_CIPHER *cipher = selectCipher(key.size());
            if (!cipher)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key length for AES-GCM");
                return Status::INVALID_ARGUMENT;
            }

            Status status = generateRandomBytes(iv_out, kIvSize, ctx);
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

            int len = 0;
            int ciphertext_len = 0;
            ciphertext_out.assign(plaintext.size(), 0);

            if (EVP_EncryptInit_ex(ctx_raw, cipher, nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_SET_IVLEN,
                                    static_cast<int>(iv_out.size()), nullptr) != 1 ||
                EVP_EncryptInit_ex(ctx_raw, nullptr, nullptr, key.data(), iv_out.data()) != 1 ||
                EVP_EncryptUpdate(ctx_raw, ciphertext_out.data(), &len,
                                  plaintext.data(), static_cast<int>(plaintext.size())) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM encryption failed");
                return Status::INTERNAL_ERROR;
            }

            ciphertext_len = len;
            if (EVP_EncryptFinal_ex(ctx_raw, ciphertext_out.data() + len, &len) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM finalization failed");
                return Status::INTERNAL_ERROR;
            }
            ciphertext_len += len;
            ciphertext_out.resize(static_cast<size_t>(ciphertext_len));

            tag_out.assign(kTagSize, 0);
            if (EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_GET_TAG,
                                    static_cast<int>(tag_out.size()), tag_out.data()) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM tag extraction failed");
                return Status::INTERNAL_ERROR;
            }

            EVP_CIPHER_CTX_free(ctx_raw);
            return Status::OK;
#else
            (void)key;
            (void)plaintext;
            (void)iv_out;
            (void)tag_out;
            (void)ciphertext_out;
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for AES-GCM");
            return Status::NOT_SUPPORTED;
#endif
        }

        Status decryptAesGcm(const std::vector<uint8_t> &key,
                             const std::vector<uint8_t> &iv,
                             const std::vector<uint8_t> &tag,
                             const std::vector<uint8_t> &ciphertext,
                             std::vector<uint8_t> &plaintext_out,
                             ErrorContext *ctx)
        {
#ifdef HAVE_OPENSSL_CRYPTO
            const EVP_CIPHER *cipher = selectCipher(key.size());
            if (!cipher)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key length for AES-GCM");
                return Status::INVALID_ARGUMENT;
            }

            EVP_CIPHER_CTX *ctx_raw = EVP_CIPHER_CTX_new();
            if (!ctx_raw)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate cipher context");
                return Status::OOM;
            }

            int len = 0;
            int plaintext_len = 0;
            plaintext_out.assign(ciphertext.size(), 0);

            if (EVP_DecryptInit_ex(ctx_raw, cipher, nullptr, nullptr, nullptr) != 1 ||
                EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_SET_IVLEN,
                                    static_cast<int>(iv.size()), nullptr) != 1 ||
                EVP_DecryptInit_ex(ctx_raw, nullptr, nullptr, key.data(), iv.data()) != 1 ||
                EVP_DecryptUpdate(ctx_raw, plaintext_out.data(), &len,
                                  ciphertext.data(), static_cast<int>(ciphertext.size())) != 1)
            {
                EVP_CIPHER_CTX_free(ctx_raw);
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "AES-GCM decryption failed");
                return Status::INTERNAL_ERROR;
            }

            plaintext_len = len;
            if (EVP_CIPHER_CTX_ctrl(ctx_raw, EVP_CTRL_GCM_SET_TAG,
                                    static_cast<int>(tag.size()),
                                    const_cast<uint8_t *>(tag.data())) != 1)
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
#else
            (void)key;
            (void)iv;
            (void)tag;
            (void)ciphertext;
            (void)plaintext_out;
            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "OpenSSL required for AES-GCM");
            return Status::NOT_SUPPORTED;
#endif
        }

        std::string buildWrappedKeyBlob(const std::vector<uint8_t> &iv,
                                        const std::vector<uint8_t> &tag,
                                        const std::vector<uint8_t> &ciphertext,
                                        uint32_t kdf_iterations)
        {
            std::string blob;
            appendUint8(blob, kWrappedKeyVersion);
            appendUint8(blob, static_cast<uint8_t>(iv.size()));
            appendUint8(blob, static_cast<uint8_t>(tag.size()));
            appendUint8(blob, 0);
            appendUint32(blob, kdf_iterations);
            appendUint32(blob, static_cast<uint32_t>(ciphertext.size()));
            blob.append(reinterpret_cast<const char *>(iv.data()), iv.size());
            blob.append(reinterpret_cast<const char *>(tag.data()), tag.size());
            blob.append(reinterpret_cast<const char *>(ciphertext.data()), ciphertext.size());
            return blob;
        }

        Status parseWrappedKeyBlob(const std::vector<uint8_t> &blob,
                                   std::vector<uint8_t> &iv_out,
                                   std::vector<uint8_t> &tag_out,
                                   std::vector<uint8_t> &ciphertext_out,
                                   uint32_t &kdf_iterations_out,
                                   ErrorContext *ctx)
        {
            std::string data(reinterpret_cast<const char *>(blob.data()), blob.size());
            size_t offset = 0;
            uint8_t version = 0;
            uint8_t iv_len = 0;
            uint8_t tag_len = 0;
            uint8_t reserved = 0;
            uint32_t ciphertext_len = 0;

            if (!readUint8(data, offset, version) || version != kWrappedKeyVersion ||
                !readUint8(data, offset, iv_len) ||
                !readUint8(data, offset, tag_len) ||
                !readUint8(data, offset, reserved) ||
                !readUint32(data, offset, kdf_iterations_out) ||
                !readUint32(data, offset, ciphertext_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid wrapped key payload");
                return Status::DATA_CORRUPTED;
            }
            (void)reserved;

            size_t needed = static_cast<size_t>(iv_len) + static_cast<size_t>(tag_len) + ciphertext_len;
            if (offset + needed != data.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Wrapped key payload size mismatch");
                return Status::DATA_CORRUPTED;
            }

            iv_out.assign(reinterpret_cast<const uint8_t *>(data.data() + offset),
                          reinterpret_cast<const uint8_t *>(data.data() + offset + iv_len));
            offset += iv_len;
            tag_out.assign(reinterpret_cast<const uint8_t *>(data.data() + offset),
                           reinterpret_cast<const uint8_t *>(data.data() + offset + tag_len));
            offset += tag_len;
            ciphertext_out.assign(reinterpret_cast<const uint8_t *>(data.data() + offset),
                                  reinterpret_cast<const uint8_t *>(data.data() + offset + ciphertext_len));

            return Status::OK;
        }

        size_t algorithmKeyLength(EncryptionAlgorithm algo)
        {
            switch (algo)
            {
            case EncryptionAlgorithm::AES128_GCM:
                return 16;
            case EncryptionAlgorithm::AES256_GCM:
                return 32;
            default:
                return 0;
            }
        }

        uint64_t runtimeNowTicks()
        {
            return static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        }

        bool isZeroUuid(const ID &value)
        {
            return value == ID{};
        }

        enum class ExternalProviderBridgeState
        {
            READY,
            UNAVAILABLE,
            UNAUTHORIZED,
            TIMEOUT
        };

        struct ExternalProviderEvaluation
        {
            bool attempted = false;
            bool available = false;
            bool authorized = false;
            CatalogManager::UnlockResult unlock_result =
                CatalogManager::UnlockResult::NOT_ATTEMPTED;
            uint64_t event_time_utc = 0;
        };

        std::string normalizeUnlockPolicy(std::string value)
        {
            value.erase(
                value.begin(),
                std::find_if(
                    value.begin(),
                    value.end(),
                    [](unsigned char ch) { return std::isspace(ch) == 0; }));
            value.erase(
                std::find_if(
                    value.rbegin(),
                    value.rend(),
                    [](unsigned char ch) { return std::isspace(ch) == 0; })
                    .base(),
                value.end());
            for (char &ch : value)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        }

        auto splitEnvList(const std::string &value) -> std::vector<std::string>
        {
            std::vector<std::string> tokens;
            size_t start = 0;
            while (start <= value.size())
            {
                const size_t end = value.find(',', start);
                const std::string token = (end == std::string::npos)
                    ? value.substr(start)
                    : value.substr(start, end - start);
                const std::string normalized = normalizeUnlockPolicy(token);
                if (!normalized.empty())
                {
                    tokens.push_back(normalized);
                }
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return tokens;
        }

        auto getEnvString(const char *key) -> std::string
        {
            const char *value = std::getenv(key);
            return value == nullptr ? std::string{} : std::string(value);
        }

        auto parseUnsignedEnv(const char *key, uint32_t fallback) -> uint32_t
        {
            const std::string value = normalizeUnlockPolicy(getEnvString(key));
            if (value.empty())
            {
                return fallback;
            }

            uint64_t parsed = 0;
            for (char ch : value)
            {
                if (ch < '0' || ch > '9')
                {
                    return fallback;
                }
                parsed = (parsed * 10ULL) + static_cast<uint64_t>(ch - '0');
                if (parsed > std::numeric_limits<uint32_t>::max())
                {
                    return fallback;
                }
            }
            return static_cast<uint32_t>(parsed);
        }

        auto providerStatusEnvKey(CatalogManager::KeyProviderKind provider) -> const char *
        {
            switch (provider)
            {
            case CatalogManager::KeyProviderKind::EXTERNAL_KMS:
                return "SCRATCHBIRD_KMS_PROVIDER_STATUS";
            case CatalogManager::KeyProviderKind::PKCS11_HSM:
                return "SCRATCHBIRD_HSM_PROVIDER_STATUS";
            case CatalogManager::KeyProviderKind::LOCAL_FILE_KEYSTORE:
            case CatalogManager::KeyProviderKind::OS_KEYRING:
                break;
            }
            return "";
        }

        auto providerAllowedKeysEnvKey(CatalogManager::KeyProviderKind provider) -> const char *
        {
            switch (provider)
            {
            case CatalogManager::KeyProviderKind::EXTERNAL_KMS:
                return "SCRATCHBIRD_KMS_ALLOWED_KEY_IDS";
            case CatalogManager::KeyProviderKind::PKCS11_HSM:
                return "SCRATCHBIRD_HSM_ALLOWED_KEY_IDS";
            case CatalogManager::KeyProviderKind::LOCAL_FILE_KEYSTORE:
            case CatalogManager::KeyProviderKind::OS_KEYRING:
                break;
            }
            return "";
        }

        auto parseExternalProviderBridgeState(const std::string &token)
            -> ExternalProviderBridgeState
        {
            const std::string normalized = normalizeUnlockPolicy(token);
            if (normalized == "ready" ||
                normalized == "ok" ||
                normalized == "success")
            {
                return ExternalProviderBridgeState::READY;
            }
            if (normalized == "unauthorized" ||
                normalized == "denied" ||
                normalized == "forbidden")
            {
                return ExternalProviderBridgeState::UNAUTHORIZED;
            }
            if (normalized == "timeout" ||
                normalized == "timed_out")
            {
                return ExternalProviderBridgeState::TIMEOUT;
            }
            return ExternalProviderBridgeState::UNAVAILABLE;
        }

        bool providerAuthorizesActiveKey(CatalogManager::KeyProviderKind provider,
                                         const ID &active_key_id)
        {
            const std::string configured = getEnvString(providerAllowedKeysEnvKey(provider));
            if (configured.empty())
            {
                return true;
            }

            const std::string active_key = normalizeUnlockPolicy(active_key_id.toString());
            const std::vector<std::string> allowed = splitEnvList(configured);
            return std::any_of(
                allowed.begin(),
                allowed.end(),
                [&active_key](const std::string &candidate) {
                    return candidate == "*" || candidate == active_key;
                });
        }

        auto evaluateExternalProviderBridge(CatalogManager::KeyProviderKind provider,
                                            const ID &active_key_id,
                                            uint32_t unlock_timeout_ms)
            -> ExternalProviderEvaluation
        {
            ExternalProviderEvaluation evaluation{};
            evaluation.attempted = true;

            const uint32_t retry_max =
                parseUnsignedEnv("SCRATCHBIRD_ENCRYPTION_UNLOCK_RETRY_MAX", 0);
            const uint32_t backoff_ms =
                parseUnsignedEnv("SCRATCHBIRD_ENCRYPTION_UNLOCK_BACKOFF_MS", 0);
            const std::vector<std::string> statuses =
                splitEnvList(getEnvString(providerStatusEnvKey(provider)));
            const auto started_at = std::chrono::steady_clock::now();
            auto elapsed_ms = [&]() -> uint64_t {
                return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started_at)
                        .count());
            };
            auto state_for_attempt = [&](uint32_t attempt) -> ExternalProviderBridgeState {
                if (statuses.empty())
                {
                    return ExternalProviderBridgeState::UNAVAILABLE;
                }
                const size_t index =
                    std::min<size_t>(attempt, statuses.size() - 1);
                return parseExternalProviderBridgeState(statuses[index]);
            };

            for (uint32_t attempt = 0; attempt <= retry_max; ++attempt)
            {
                if (unlock_timeout_ms > 0 && elapsed_ms() >= unlock_timeout_ms)
                {
                    evaluation.unlock_result = CatalogManager::UnlockResult::TIMED_OUT;
                    evaluation.event_time_utc = runtimeNowTicks();
                    return evaluation;
                }

                switch (state_for_attempt(attempt))
                {
                case ExternalProviderBridgeState::READY:
                    evaluation.available = true;
                    evaluation.authorized =
                        providerAuthorizesActiveKey(provider, active_key_id);
                    evaluation.unlock_result = evaluation.authorized
                        ? CatalogManager::UnlockResult::SUCCESS
                        : CatalogManager::UnlockResult::FAILED;
                    evaluation.event_time_utc = runtimeNowTicks();
                    return evaluation;
                case ExternalProviderBridgeState::UNAUTHORIZED:
                    evaluation.available = true;
                    evaluation.authorized = false;
                    evaluation.unlock_result = CatalogManager::UnlockResult::FAILED;
                    evaluation.event_time_utc = runtimeNowTicks();
                    return evaluation;
                case ExternalProviderBridgeState::UNAVAILABLE:
                    evaluation.available = false;
                    evaluation.authorized = false;
                    if (attempt == retry_max)
                    {
                        evaluation.unlock_result = CatalogManager::UnlockResult::FAILED;
                        evaluation.event_time_utc = runtimeNowTicks();
                        return evaluation;
                    }
                    break;
                case ExternalProviderBridgeState::TIMEOUT:
                    evaluation.available = false;
                    evaluation.authorized = false;
                    if (attempt == retry_max)
                    {
                        evaluation.unlock_result = CatalogManager::UnlockResult::TIMED_OUT;
                        evaluation.event_time_utc = runtimeNowTicks();
                        return evaluation;
                    }
                    break;
                }

                if (backoff_ms > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                }
            }

            evaluation.unlock_result = CatalogManager::UnlockResult::FAILED;
            evaluation.event_time_utc = runtimeNowTicks();
            return evaluation;
        }

        Status persistBootstrapUnlockOutcome(
            CatalogManager *catalog,
            CatalogManager::EncryptionBootstrapInfoCatalogInfo &bootstrap,
            const ExternalProviderEvaluation &evaluation,
            ErrorContext *ctx)
        {
            if (catalog == nullptr || !evaluation.attempted)
            {
                return Status::OK;
            }

            bootstrap.last_unlock_result = evaluation.unlock_result;
            bootstrap.has_last_unlock_time = true;
            bootstrap.last_unlock_time =
                (evaluation.event_time_utc == 0) ? runtimeNowTicks() : evaluation.event_time_utc;
            return catalog->upsertEncryptionBootstrapInfoCatalogEntry(bootstrap, ctx);
        }

        Status configureBootstrapPolicyRequest(
            CatalogManager *catalog,
            const CatalogManager::EncryptionProfileCatalogInfo &profile,
            CatalogManager::EncryptionBootstrapInfoCatalogInfo &bootstrap,
            const CatalogManager::EncryptionKeyCatalogInfo &active_key,
            CatalogManager::CryptoBaselineEvaluationRequest &request_out,
            ErrorContext *ctx)
        {
            request_out = CatalogManager::CryptoBaselineEvaluationRequest{};
            request_out.evaluate_rotation_window = true;
            request_out.privileged_operation = true;
            request_out.has_encryption_profile_id = true;
            request_out.encryption_profile_id = profile.profile_id;
            request_out.has_active_key_id = true;
            request_out.active_key_id = active_key.key_id;
            request_out.now_utc = runtimeNowTicks();
            request_out.artifact_signed = true;
            request_out.artifact_signature_algorithm = "Ed25519";
            request_out.primary_provider_available = true;
            request_out.primary_provider_authorized = true;
            request_out.nonce_len_bytes = 12;
            request_out.aes_gcm_hw_available = DataEncryption::hasHardwareAcceleration();

            switch (profile.cipher)
            {
            case CatalogManager::EncryptionAlgorithm::AES_256_GCM:
                request_out.crypto_profile_id = CatalogManager::CryptoProfileId::MODERN_BASELINE;
                request_out.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::AES_256_GCM;
                break;
            case CatalogManager::EncryptionAlgorithm::CHACHA20_POLY1305:
                request_out.crypto_profile_id = CatalogManager::CryptoProfileId::COMPAT_EMULATION;
                request_out.at_rest_algorithm = CatalogManager::EncryptionAlgorithm::CHACHA20_POLY1305;
                request_out.chacha_fallback_explicit = true;
                break;
            default:
                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::INVALID_AUTHORIZATION,
                    "SEC_1292",
                    "Encryption profile uses unsupported at-rest algorithm");
                return Status::INVALID_AUTHORIZATION;
            }

            const std::string policy = normalizeUnlockPolicy(bootstrap.unlock_policy);
            if (policy == "local_only" ||
                policy == "local_file" ||
                policy == "local_keystore")
            {
                request_out.security_tier = CatalogManager::SecurityTierId::TIER_1_BASIC;
                request_out.primary_provider = CatalogManager::KeyProviderKind::LOCAL_FILE_KEYSTORE;
                return Status::OK;
            }
            if (policy == "os_keyring" ||
                policy == "os_keyring_primary" ||
                policy == "manual_quorum")
            {
                request_out.security_tier = CatalogManager::SecurityTierId::TIER_2_STANDARD;
                request_out.primary_provider = CatalogManager::KeyProviderKind::OS_KEYRING;
                return Status::OK;
            }
            if (policy == "external_kms" || policy == "kms_first")
            {
                request_out.security_tier = CatalogManager::SecurityTierId::TIER_2_STANDARD;
                request_out.primary_provider = CatalogManager::KeyProviderKind::EXTERNAL_KMS;
                const ExternalProviderEvaluation evaluation =
                    evaluateExternalProviderBridge(
                        request_out.primary_provider,
                        active_key.key_id,
                        bootstrap.unlock_timeout_ms);
                request_out.primary_provider_available = evaluation.available;
                request_out.primary_provider_authorized = evaluation.authorized;
                const Status persist_status =
                    persistBootstrapUnlockOutcome(catalog, bootstrap, evaluation, ctx);
                if (persist_status != Status::OK)
                {
                    return persist_status;
                }
                if (evaluation.unlock_result == CatalogManager::UnlockResult::SUCCESS)
                {
                    return Status::OK;
                }
                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::CONNECTION_FAILURE,
                    "SEC_1293",
                    (evaluation.unlock_result == CatalogManager::UnlockResult::TIMED_OUT)
                        ? "External KMS provider timed out during database unlock"
                        : "External KMS provider is unavailable or unauthorized for the active key");
                return Status::CONNECTION_FAILURE;
            }
            if (policy == "hsm_quorum")
            {
                if (bootstrap.min_shards_required < 2)
                {
                    SET_ERROR_CONTEXT_VNEXT(
                        ctx,
                        Status::INVALID_AUTHORIZATION,
                        "SEC_1292",
                        "HSM quorum policy requires at least two bootstrap shards");
                    return Status::INVALID_AUTHORIZATION;
                }
                request_out.security_tier = CatalogManager::SecurityTierId::TIER_3_HARDENED;
                request_out.primary_provider = CatalogManager::KeyProviderKind::PKCS11_HSM;
                const ExternalProviderEvaluation evaluation =
                    evaluateExternalProviderBridge(
                        request_out.primary_provider,
                        active_key.key_id,
                        bootstrap.unlock_timeout_ms);
                request_out.primary_provider_available = evaluation.available;
                request_out.primary_provider_authorized = evaluation.authorized;
                const Status persist_status =
                    persistBootstrapUnlockOutcome(catalog, bootstrap, evaluation, ctx);
                if (persist_status != Status::OK)
                {
                    return persist_status;
                }
                if (evaluation.unlock_result == CatalogManager::UnlockResult::SUCCESS)
                {
                    return Status::OK;
                }
                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::CONNECTION_FAILURE,
                    "SEC_1293",
                    (evaluation.unlock_result == CatalogManager::UnlockResult::TIMED_OUT)
                        ? "PKCS#11 HSM provider timed out during database unlock"
                        : "PKCS#11 HSM provider is unavailable or unauthorized for the active key");
                return Status::CONNECTION_FAILURE;
            }
            if (policy == "kms_hsm_primary")
            {
                if (bootstrap.min_shards_required < 2)
                {
                    SET_ERROR_CONTEXT_VNEXT(
                        ctx,
                        Status::INVALID_AUTHORIZATION,
                        "SEC_1292",
                        "kms_hsm_primary requires a quorum-aware bootstrap shard threshold");
                    return Status::INVALID_AUTHORIZATION;
                }

                request_out.security_tier = CatalogManager::SecurityTierId::TIER_3_HARDENED;
                request_out.primary_provider = CatalogManager::KeyProviderKind::EXTERNAL_KMS;
                request_out.has_escrow_provider = true;
                request_out.escrow_provider = CatalogManager::KeyProviderKind::PKCS11_HSM;

                const ExternalProviderEvaluation kms_evaluation =
                    evaluateExternalProviderBridge(
                        request_out.primary_provider,
                        active_key.key_id,
                        bootstrap.unlock_timeout_ms);
                request_out.primary_provider_available = kms_evaluation.available;
                request_out.primary_provider_authorized = kms_evaluation.authorized;
                if (kms_evaluation.unlock_result != CatalogManager::UnlockResult::SUCCESS)
                {
                    const Status persist_status =
                        persistBootstrapUnlockOutcome(catalog, bootstrap, kms_evaluation, ctx);
                    if (persist_status != Status::OK)
                    {
                        return persist_status;
                    }
                    SET_ERROR_CONTEXT_VNEXT(
                        ctx,
                        Status::CONNECTION_FAILURE,
                        "SEC_1293",
                        (kms_evaluation.unlock_result == CatalogManager::UnlockResult::TIMED_OUT)
                            ? "Primary external KMS provider timed out during database unlock"
                            : "Primary external KMS provider is unavailable or unauthorized for the active key");
                    return Status::CONNECTION_FAILURE;
                }

                const ExternalProviderEvaluation hsm_evaluation =
                    evaluateExternalProviderBridge(
                        request_out.escrow_provider,
                        active_key.key_id,
                        bootstrap.unlock_timeout_ms);
                request_out.escrow_provider_available = hsm_evaluation.available;
                request_out.escrow_provider_authorized = hsm_evaluation.authorized;
                const Status persist_status =
                    persistBootstrapUnlockOutcome(catalog, bootstrap, hsm_evaluation, ctx);
                if (persist_status != Status::OK)
                {
                    return persist_status;
                }
                if (hsm_evaluation.unlock_result == CatalogManager::UnlockResult::SUCCESS)
                {
                    return Status::OK;
                }

                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::CONNECTION_FAILURE,
                    "SEC_1293",
                    (hsm_evaluation.unlock_result == CatalogManager::UnlockResult::TIMED_OUT)
                        ? "Companion PKCS#11 HSM attestation timed out during database unlock"
                        : "Companion PKCS#11 HSM attestation is unavailable or unauthorized for the active key");
                return Status::CONNECTION_FAILURE;
            }

            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1292",
                "Encryption bootstrap unlock policy is unknown");
            return Status::INVALID_AUTHORIZATION;
        }

        void copyErrorContext(const ErrorContext &from, ErrorContext *to)
        {
            if (to == nullptr)
            {
                return;
            }

            to->code = from.code;
            to->sqlstate_text = from.sqlstate_text;
            to->sqlstate = to->sqlstate_text.empty() ? from.sqlstate : to->sqlstate_text.c_str();
            to->message = from.message;
            to->vnext_code = from.vnext_code;
            to->file = from.file;
            to->line = from.line;
            to->function = from.function;
            to->constraint_name = from.constraint_name;
            to->table_name = from.table_name;
            to->column_name = from.column_name;
            to->violating_value = from.violating_value;
            to->referenced_table = from.referenced_table;
            to->referenced_column = from.referenced_column;
            to->check_expression = from.check_expression;
            to->hint = from.hint;
        }
    }

    struct EncryptionKeyManager::EncryptionKeyRecord
    {
        ID key_id;
        ID domain_id;
        uint8_t algorithm;
        uint8_t is_active;
        uint8_t is_master;
        uint8_t is_valid;
        uint32_t key_version;
        uint64_t created_at;
        uint64_t rotated_at;
        ID encrypted_key_oid;
        ID key_salt_oid;
        uint32_t reserved;

        EncryptionKeyRecord()
            : algorithm(0),
              is_active(0),
              is_master(0),
              is_valid(1),
              key_version(0),
              created_at(0),
              rotated_at(0),
              encrypted_key_oid(),
              key_salt_oid(),
              reserved(0)
        {
        }
    };

    EncryptionKeyManager::EncryptionKeyManager(Database *db)
        : db_(db)
    {
    }

    EncryptionKeyManager::~EncryptionKeyManager() = default;

    Status EncryptionKeyManager::initialize(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return ensureKeysTable(ctx);
    }

    Status EncryptionKeyManager::ensureKeysTable(ErrorContext *ctx)
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        Status status = catalog->ensureEncryptionKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        keys_table_page_ = catalog->encryptionKeysTablePage();
        if (keys_table_page_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Encryption keys table not initialized");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    Status EncryptionKeyManager::ensureMasterKeyLoaded(bool create_if_missing, ErrorContext *ctx)
    {
        if (master_key_loaded_)
        {
            return Status::OK;
        }

        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptionKeyRecord master_record;
        uint32_t slot = 0;
        status = findMasterKeyRecord(master_record, slot, ctx);
        if (status == Status::NOT_FOUND)
        {
            if (!create_if_missing)
            {
                return Status::NOT_FOUND;
            }
            std::vector<uint8_t> generated_key;
            status = generateRandomBytes(generated_key, 32, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            return setMasterKeyUnlocked(generated_key, ctx);
        }
        if (status != Status::OK)
        {
            return status;
        }

        EncryptionKey master_key_record;
        status = loadKeyFromRecord(master_record, master_key_record, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> iv;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> ciphertext;
        uint32_t kdf_iterations = 0;
        status = parseWrappedKeyBlob(master_key_record.encrypted_key, iv, tag, ciphertext,
                                     kdf_iterations, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> uuid_bytes(db_->uuid().bytes.begin(), db_->uuid().bytes.end());
        std::vector<uint8_t> master_kek;
        status = deriveKeyPBKDF2(uuid_bytes, master_key_record.key_salt,
                                 kdf_iterations, 32,
                                 master_kek, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> master_plain;
        status = decryptAesGcm(master_kek, iv, tag, ciphertext, master_plain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        master_key_ = std::move(master_plain);
        master_key_loaded_ = true;
        return Status::OK;
    }

    Status EncryptionKeyManager::generateKey(const ID &domain_id, EncryptionAlgorithm algo,
                                            ID &key_id_out, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return generateKeyUnlocked(domain_id, algo, key_id_out, ctx);
    }

    Status EncryptionKeyManager::rotateKey(const ID &domain_id, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        EncryptionKeyRecord active_record;
        uint32_t active_slot = 0;
        Status status = findActiveKeyRecord(domain_id, active_record, active_slot, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        active_record.is_active = 0;
        active_record.rotated_at = std::chrono::system_clock::now().time_since_epoch().count();
        status = writeKeyRecord(active_record, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        ID new_key_id;
        return generateKeyUnlocked(domain_id, static_cast<EncryptionAlgorithm>(active_record.algorithm),
                                   new_key_id, ctx);
    }

    Status EncryptionKeyManager::deleteKey(const ID &key_id, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        EncryptionKeyRecord record;
        uint32_t slot = 0;
        Status status = findKeyRecord(key_id, record, slot, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        record.is_valid = 0;
        record.is_active = 0;
        return writeKeyRecord(record, ctx);
    }

    Status EncryptionKeyManager::validateDatabaseEncryptionPolicy(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::EncryptionBootstrapInfoCatalogInfo bootstrap{};
        Status status = catalog->getEncryptionBootstrapInfoCatalogEntry(db_->uuid(), bootstrap, ctx);
        if (status == Status::NOT_FOUND)
        {
            return Status::OK;
        }
        if (status != Status::OK)
        {
            return status;
        }

        if (bootstrap.database_id != db_->uuid() ||
            isZeroUuid(bootstrap.profile_id) ||
            isZeroUuid(bootstrap.active_key_id))
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1292",
                "Encryption bootstrap metadata is incomplete");
            return Status::INVALID_AUTHORIZATION;
        }

        CatalogManager::EncryptionProfileCatalogInfo profile{};
        status = catalog->getEncryptionProfileCatalogEntry(bootstrap.profile_id, profile, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1292",
                "Encryption bootstrap profile is missing");
            return Status::INVALID_AUTHORIZATION;
        }
        if (!profile.is_active)
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1292",
                "Encryption bootstrap profile is inactive");
            return Status::INVALID_AUTHORIZATION;
        }
        if (bootstrap.min_shards_required < profile.min_shards_required)
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1292",
                "Encryption bootstrap quorum is weaker than the profile minimum");
            return Status::INVALID_AUTHORIZATION;
        }

        CatalogManager::EncryptionKeyCatalogInfo active_key{};
        status = catalog->getEncryptionKeyCatalogEntry(bootstrap.active_key_id, active_key, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1294",
                "Encryption bootstrap active key is missing");
            return Status::INVALID_AUTHORIZATION;
        }
        if (active_key.profile_id != bootstrap.profile_id ||
            active_key.key_status != CatalogManager::EncryptionKeyStatus::ACTIVE ||
            !active_key.has_activated_time)
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1294",
                "Encryption bootstrap active key is not valid for runtime activation");
            return Status::INVALID_AUTHORIZATION;
        }

        CatalogManager::CryptoBaselineEvaluationRequest request{};
        status = configureBootstrapPolicyRequest(
            catalog, profile, bootstrap, active_key, request, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        CatalogManager::CryptoBaselineEvaluationDecision decision{};
        status = catalog->evaluateCryptoBaselinePolicy(request, decision, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (decision.rotation_overdue)
        {
            SET_ERROR_CONTEXT_VNEXT(
                ctx,
                Status::INVALID_AUTHORIZATION,
                "SEC_1294",
                "Database encryption key rotation is overdue");
            return Status::INVALID_AUTHORIZATION;
        }

        return Status::OK;
    }

    Status EncryptionKeyManager::rotateDatabaseKey(const ID &key_id, ErrorContext *ctx)
    {
        if (isZeroUuid(key_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database encryption key id is required");
            return Status::INVALID_ARGUMENT;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not available");
                return Status::INVALID_ARGUMENT;
            }

            CatalogManager *catalog = db_->catalog_manager();
            if (!catalog)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
                return Status::INVALID_ARGUMENT;
            }

            CatalogManager::EncryptionBootstrapInfoCatalogInfo bootstrap{};
            Status status = catalog->getEncryptionBootstrapInfoCatalogEntry(db_->uuid(), bootstrap, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            CatalogManager::EncryptionKeyCatalogInfo target_key{};
            status = catalog->getEncryptionKeyCatalogEntry(key_id, target_key, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            if (target_key.profile_id != bootstrap.profile_id)
            {
                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::INVALID_ARGUMENT,
                    "SEC_1294",
                    "Database encryption key does not belong to the bootstrap profile");
                return Status::INVALID_ARGUMENT;
            }
            if (target_key.key_status != CatalogManager::EncryptionKeyStatus::STAGED &&
                target_key.key_status != CatalogManager::EncryptionKeyStatus::ACTIVE)
            {
                SET_ERROR_CONTEXT_VNEXT(
                    ctx,
                    Status::INVALID_ARGUMENT,
                    "SEC_1294",
                    "Database encryption key must be staged or active before rotation");
                return Status::INVALID_ARGUMENT;
            }

            if (target_key.key_status == CatalogManager::EncryptionKeyStatus::STAGED)
            {
                CatalogManager::EncryptionKeyLifecycleTransitionRequest transition{};
                transition.key_id = key_id;
                transition.target_status = CatalogManager::EncryptionKeyStatus::ACTIVE;
                transition.event_time_utc = runtimeNowTicks();
                transition.retire_existing_active = true;
                CatalogManager::EncryptionKeyLifecycleTransitionDecision decision{};
                status = catalog->transitionEncryptionKeyLifecycle(transition, decision, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            bootstrap.active_key_id = key_id;
            status = catalog->upsertEncryptionBootstrapInfoCatalogEntry(bootstrap, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        ErrorContext local_ctx;
        const Status status = validateDatabaseEncryptionPolicy(&local_ctx);
        if (status != Status::OK)
        {
            copyErrorContext(local_ctx, ctx);
            return status;
        }

        return Status::OK;
    }

    Status EncryptionKeyManager::getActiveKey(const ID &domain_id, EncryptionKey &key_out,
                                             ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        EncryptionKeyRecord record;
        uint32_t slot = 0;
        Status status = findActiveKeyRecord(domain_id, record, slot, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return loadKeyFromRecord(record, key_out, ctx);
    }

    Status EncryptionKeyManager::getKeyByVersion(const ID &domain_id, uint32_t version,
                                                EncryptionKey &key_out, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        EncryptionKeyRecord record;
        uint32_t slot = 0;
        Status status = findKeyByVersionRecord(domain_id, version, record, slot, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return loadKeyFromRecord(record, key_out, ctx);
    }

    Status EncryptionKeyManager::decryptKey(const EncryptionKey &encrypted_key,
                                           std::vector<uint8_t> &plaintext_key_out,
                                           ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = ensureMasterKeyLoaded(false, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> iv;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> ciphertext;
        uint32_t kdf_iterations = 0;
        status = parseWrappedKeyBlob(encrypted_key.encrypted_key, iv, tag, ciphertext,
                                     kdf_iterations, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        size_t key_len = algorithmKeyLength(encrypted_key.algorithm);
        if (key_len == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported encryption algorithm");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<uint8_t> wrapping_key;
        status = deriveKeyPBKDF2(master_key_, encrypted_key.key_salt,
                                 kdf_iterations, key_len, wrapping_key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return decryptAesGcm(wrapping_key, iv, tag, ciphertext, plaintext_key_out, ctx);
    }

    Status EncryptionKeyManager::encryptWithMasterKey(const std::vector<uint8_t> &plaintext,
                                                     std::vector<uint8_t> &encrypted_out,
                                                     ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = ensureMasterKeyLoaded(true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptedValue encrypted;
        status = DataEncryption::encrypt(plaintext, master_key_,
                                         EncryptionAlgorithm::AES256_GCM,
                                         encrypted, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return serializeEncryptedValue(encrypted, encrypted_out, ctx);
    }

    Status EncryptionKeyManager::decryptWithMasterKey(const std::vector<uint8_t> &encrypted,
                                                     std::vector<uint8_t> &plaintext_out,
                                                     ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = ensureMasterKeyLoaded(false, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptedValue parsed;
        status = parseEncryptedValue(encrypted, parsed, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return DataEncryption::decrypt(parsed, master_key_, plaintext_out, ctx);
    }

    Status EncryptionKeyManager::setMasterKey(const std::vector<uint8_t> &master_key,
                                             ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return setMasterKeyUnlocked(master_key, ctx);
    }

    Status EncryptionKeyManager::initializeMasterKey(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return ensureMasterKeyLoaded(true, ctx);
    }

    Status EncryptionKeyManager::loadKeyFromRecord(const EncryptionKeyRecord &record,
                                                   EncryptionKey &key_out,
                                                   ErrorContext *ctx)
    {
        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        uint64_t xmin = ConnectionContext::getCurrentTransactionId();
        std::string wrapped_blob;
        std::string salt_blob;

        Status status = catalog->loadStringFromToast(record.encrypted_key_oid, xmin,
                                                     wrapped_blob, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = catalog->loadStringFromToast(record.key_salt_oid, xmin, salt_blob, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        key_out.key_id = record.key_id;
        key_out.domain_id = record.domain_id;
        key_out.algorithm = static_cast<EncryptionAlgorithm>(record.algorithm);
        key_out.key_version = record.key_version;
        key_out.created_at = record.created_at;
        key_out.rotated_at = record.rotated_at;
        key_out.active = record.is_active != 0;
        key_out.encrypted_key.assign(wrapped_blob.begin(), wrapped_blob.end());
        key_out.key_salt.assign(salt_blob.begin(), salt_blob.end());

        return Status::OK;
    }

    Status EncryptionKeyManager::findKeyRecord(const ID &key_id,
                                               EncryptionKeyRecord &record_out,
                                               uint32_t &slot_out,
                                               ErrorContext *ctx)
    {
        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        status = bp->pinPage(keys_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        for (uint32_t i = 0; i < page->record_count; ++i)
        {
            auto *record = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (i * sizeof(EncryptionKeyRecord)));
            if (record->is_valid && record->key_id == key_id)
            {
                record_out = *record;
                slot_out = i;
                bp->unpinPage(keys_table_page_, false, ctx);
                return Status::OK;
            }
        }

        bp->unpinPage(keys_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Encryption key not found");
        return Status::NOT_FOUND;
    }

    Status EncryptionKeyManager::findActiveKeyRecord(const ID &domain_id,
                                                    EncryptionKeyRecord &record_out,
                                                    uint32_t &slot_out,
                                                    ErrorContext *ctx)
    {
        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        status = bp->pinPage(keys_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        for (uint32_t i = 0; i < page->record_count; ++i)
        {
            auto *record = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (i * sizeof(EncryptionKeyRecord)));
            if (record->is_valid &&
                record->is_active &&
                !record->is_master &&
                record->domain_id == domain_id)
            {
                record_out = *record;
                slot_out = i;
                bp->unpinPage(keys_table_page_, false, ctx);
                return Status::OK;
            }
        }

        bp->unpinPage(keys_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Active encryption key not found");
        return Status::NOT_FOUND;
    }

    Status EncryptionKeyManager::findKeyByVersionRecord(const ID &domain_id, uint32_t version,
                                                       EncryptionKeyRecord &record_out,
                                                       uint32_t &slot_out,
                                                       ErrorContext *ctx)
    {
        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        status = bp->pinPage(keys_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        for (uint32_t i = 0; i < page->record_count; ++i)
        {
            auto *record = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (i * sizeof(EncryptionKeyRecord)));
            if (record->is_valid &&
                !record->is_master &&
                record->domain_id == domain_id &&
                record->key_version == version)
            {
                record_out = *record;
                slot_out = i;
                bp->unpinPage(keys_table_page_, false, ctx);
                return Status::OK;
            }
        }

        bp->unpinPage(keys_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Encryption key version not found");
        return Status::NOT_FOUND;
    }

    Status EncryptionKeyManager::findMasterKeyRecord(EncryptionKeyRecord &record_out,
                                                     uint32_t &slot_out,
                                                     ErrorContext *ctx)
    {
        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        status = bp->pinPage(keys_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        for (uint32_t i = 0; i < page->record_count; ++i)
        {
            auto *record = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (i * sizeof(EncryptionKeyRecord)));
            if (record->is_valid && record->is_master)
            {
                record_out = *record;
                slot_out = i;
                bp->unpinPage(keys_table_page_, false, ctx);
                return Status::OK;
            }
        }

        bp->unpinPage(keys_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Master key not found");
        return Status::NOT_FOUND;
    }

    Status EncryptionKeyManager::writeKeyRecord(const EncryptionKeyRecord &record, ErrorContext *ctx)
    {
        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        status = bp->pinPage(keys_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        EncryptionKeyRecord *target = nullptr;
        for (uint32_t i = 0; i < page->record_count; ++i)
        {
            auto *existing = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (i * sizeof(EncryptionKeyRecord)));
            if (existing->key_id == record.key_id)
            {
                target = existing;
                break;
            }
        }

        if (!target)
        {
            uint32_t capacity = (db_->page_size() - sizeof(CatalogHeapPage)) /
                                sizeof(EncryptionKeyRecord);
            if (page->record_count >= capacity)
            {
                bp->unpinPage(keys_table_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Encryption key catalog page full");
                return Status::IO_ERROR;
            }

            target = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (page->record_count * sizeof(EncryptionKeyRecord)));
            page->record_count++;
        }

        *target = record;
        return bp->unpinPage(keys_table_page_, true, ctx);
    }

    uint32_t EncryptionKeyManager::nextKeyVersion(const ID &domain_id, ErrorContext *ctx)
    {
        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return 1;
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            return 1;
        }

        void *page_buffer = nullptr;
        status = bp->pinPage(keys_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return 1;
        }

        auto *page = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        uint32_t max_version = 0;
        for (uint32_t i = 0; i < page->record_count; ++i)
        {
            auto *record = reinterpret_cast<EncryptionKeyRecord *>(
                page->data + (i * sizeof(EncryptionKeyRecord)));
            if (record->is_valid &&
                !record->is_master &&
                record->domain_id == domain_id)
            {
                max_version = std::max(max_version, record->key_version);
            }
        }

        bp->unpinPage(keys_table_page_, false, ctx);
        return max_version + 1;
    }

    Status EncryptionKeyManager::generateKeyUnlocked(const ID &domain_id, EncryptionAlgorithm algo,
                                                    ID &key_id_out, ErrorContext *ctx)
    {
        if (algorithmKeyLength(algo) == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported encryption algorithm");
            return Status::INVALID_ARGUMENT;
        }

        Status status = ensureMasterKeyLoaded(true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptionKeyRecord active_record;
        uint32_t active_slot = 0;
        status = findActiveKeyRecord(domain_id, active_record, active_slot, ctx);
        if (status == Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Active key already exists for domain");
            return Status::FILE_EXISTS;
        }
        if (status != Status::NOT_FOUND)
        {
            return status;
        }

        size_t key_len = algorithmKeyLength(algo);
        std::vector<uint8_t> raw_key;
        status = generateRandomBytes(raw_key, key_len, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> salt;
        status = generateRandomBytes(salt, kSaltSize, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> wrapping_key;
        status = deriveKeyPBKDF2(master_key_, salt, kDefaultKdfIterations, key_len,
                                 wrapping_key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> iv;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> ciphertext;
        status = encryptAesGcm(wrapping_key, raw_key, iv, tag, ciphertext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::string wrapped_blob = buildWrappedKeyBlob(iv, tag, ciphertext, kDefaultKdfIterations);
        std::string salt_blob(reinterpret_cast<const char *>(salt.data()), salt.size());

        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        uint64_t xmin = ConnectionContext::getCurrentTransactionId();
        ID wrapped_oid{};
        ID salt_oid{};
        status = catalog->storeStringInToast(wrapped_blob, xmin, wrapped_oid, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = catalog->storeStringInToast(salt_blob, xmin, salt_oid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptionKeyRecord record;
        record.key_id = generateUuidV7();
        record.domain_id = domain_id;
        record.algorithm = static_cast<uint8_t>(algo);
        record.is_active = 1;
        record.is_master = 0;
        record.is_valid = 1;
        record.key_version = nextKeyVersion(domain_id, ctx);
        record.created_at = std::chrono::system_clock::now().time_since_epoch().count();
        record.rotated_at = 0;
        record.encrypted_key_oid = wrapped_oid;
        record.key_salt_oid = salt_oid;

        status = writeKeyRecord(record, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        key_id_out = record.key_id;
        return Status::OK;
    }

    Status EncryptionKeyManager::setMasterKeyUnlocked(const std::vector<uint8_t> &master_key,
                                                     ErrorContext *ctx)
    {
        if (master_key.size() != 32)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Master key must be 32 bytes");
            return Status::INVALID_ARGUMENT;
        }

        Status status = ensureKeysTable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> salt;
        status = generateRandomBytes(salt, kSaltSize, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> uuid_bytes(db_->uuid().bytes.begin(), db_->uuid().bytes.end());
        std::vector<uint8_t> master_kek;
        status = deriveKeyPBKDF2(uuid_bytes, salt, kDefaultKdfIterations, master_key.size(),
                                 master_kek, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> iv;
        std::vector<uint8_t> tag;
        std::vector<uint8_t> ciphertext;
        status = encryptAesGcm(master_kek, master_key, iv, tag, ciphertext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::string wrapped_blob = buildWrappedKeyBlob(iv, tag, ciphertext, kDefaultKdfIterations);
        std::string salt_blob(reinterpret_cast<const char *>(salt.data()), salt.size());

        CatalogManager *catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        uint64_t xmin = ConnectionContext::getCurrentTransactionId();
        ID wrapped_oid{};
        ID salt_oid{};
        status = catalog->storeStringInToast(wrapped_blob, xmin, wrapped_oid, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = catalog->storeStringInToast(salt_blob, xmin, salt_oid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptionKeyRecord master_record;
        uint32_t slot = 0;
        status = findMasterKeyRecord(master_record, slot, ctx);
        if (status == Status::OK)
        {
            master_record.algorithm = static_cast<uint8_t>(EncryptionAlgorithm::AES256_GCM);
            master_record.is_active = 1;
            master_record.is_master = 1;
            master_record.is_valid = 1;
            master_record.key_version += 1;
            master_record.rotated_at = std::chrono::system_clock::now().time_since_epoch().count();
            master_record.encrypted_key_oid = wrapped_oid;
            master_record.key_salt_oid = salt_oid;
        }
        else if (status == Status::NOT_FOUND)
        {
            master_record.key_id = generateUuidV7();
            master_record.domain_id = ID{};
            master_record.algorithm = static_cast<uint8_t>(EncryptionAlgorithm::AES256_GCM);
            master_record.is_active = 1;
            master_record.is_master = 1;
            master_record.is_valid = 1;
            master_record.key_version = 1;
            master_record.created_at = std::chrono::system_clock::now().time_since_epoch().count();
            master_record.rotated_at = 0;
            master_record.encrypted_key_oid = wrapped_oid;
            master_record.key_salt_oid = salt_oid;
        }
        else
        {
            return status;
        }

        status = writeKeyRecord(master_record, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        master_key_ = master_key;
        master_key_loaded_ = true;
        return Status::OK;
    }
} // namespace scratchbird::core
