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
#include <chrono>
#include <random>
#include <string>

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
        uint32_t encrypted_key_oid;
        uint32_t key_salt_oid;
        uint32_t reserved;

        EncryptionKeyRecord()
            : algorithm(0),
              is_active(0),
              is_master(0),
              is_valid(1),
              key_version(0),
              created_at(0),
              rotated_at(0),
              encrypted_key_oid(0),
              key_salt_oid(0),
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
        uint32_t wrapped_oid = 0;
        uint32_t salt_oid = 0;
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
        uint32_t wrapped_oid = 0;
        uint32_t salt_oid = 0;
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
