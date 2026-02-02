/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <vector>
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    class Database;

    using ID = UuidV7Bytes;

    enum class EncryptionAlgorithm : uint8_t
    {
        NONE = 0,
        AES128_GCM = 1,
        AES256_GCM = 2
    };

    struct EncryptionKey
    {
        ID key_id;
        ID domain_id;
        EncryptionAlgorithm algorithm = EncryptionAlgorithm::NONE;
        std::vector<uint8_t> encrypted_key;
        std::vector<uint8_t> key_salt;
        uint32_t key_version = 0;
        uint64_t created_at = 0;
        uint64_t rotated_at = 0;
        bool active = false;
    };

    class EncryptionKeyManager
    {
    public:
        explicit EncryptionKeyManager(Database *db);
        ~EncryptionKeyManager();

        Status initialize(ErrorContext *ctx = nullptr);

        Status generateKey(const ID &domain_id, EncryptionAlgorithm algo,
                           ID &key_id_out, ErrorContext *ctx = nullptr);
        Status rotateKey(const ID &domain_id, ErrorContext *ctx = nullptr);
        Status deleteKey(const ID &key_id, ErrorContext *ctx = nullptr);

        Status getActiveKey(const ID &domain_id, EncryptionKey &key_out,
                            ErrorContext *ctx = nullptr);
        Status getKeyByVersion(const ID &domain_id, uint32_t version,
                               EncryptionKey &key_out, ErrorContext *ctx = nullptr);

        Status decryptKey(const EncryptionKey &encrypted_key,
                          std::vector<uint8_t> &plaintext_key_out,
                          ErrorContext *ctx = nullptr);

        Status encryptWithMasterKey(const std::vector<uint8_t> &plaintext,
                                    std::vector<uint8_t> &encrypted_out,
                                    ErrorContext *ctx = nullptr);
        Status decryptWithMasterKey(const std::vector<uint8_t> &encrypted,
                                    std::vector<uint8_t> &plaintext_out,
                                    ErrorContext *ctx = nullptr);

        Status setMasterKey(const std::vector<uint8_t> &master_key,
                            ErrorContext *ctx = nullptr);
        Status initializeMasterKey(ErrorContext *ctx = nullptr);

    private:
        struct EncryptionKeyRecord;

        Database *db_;
        std::mutex mutex_;
        uint32_t keys_table_page_ = 0;
        std::vector<uint8_t> master_key_;
        bool master_key_loaded_ = false;

        Status ensureKeysTable(ErrorContext *ctx);
        Status ensureMasterKeyLoaded(bool create_if_missing, ErrorContext *ctx);
        Status loadKeyFromRecord(const EncryptionKeyRecord &record, EncryptionKey &key_out,
                                 ErrorContext *ctx);
        Status findKeyRecord(const ID &key_id, EncryptionKeyRecord &record_out,
                             uint32_t &slot_out, ErrorContext *ctx);
        Status findActiveKeyRecord(const ID &domain_id, EncryptionKeyRecord &record_out,
                                   uint32_t &slot_out, ErrorContext *ctx);
        Status findKeyByVersionRecord(const ID &domain_id, uint32_t version,
                                      EncryptionKeyRecord &record_out,
                                      uint32_t &slot_out, ErrorContext *ctx);
        Status findMasterKeyRecord(EncryptionKeyRecord &record_out,
                                   uint32_t &slot_out, ErrorContext *ctx);
        Status writeKeyRecord(const EncryptionKeyRecord &record, ErrorContext *ctx);
        uint32_t nextKeyVersion(const ID &domain_id, ErrorContext *ctx);
        Status generateKeyUnlocked(const ID &domain_id, EncryptionAlgorithm algo,
                                   ID &key_id_out, ErrorContext *ctx);
        Status setMasterKeyUnlocked(const std::vector<uint8_t> &master_key,
                                    ErrorContext *ctx);
    };
} // namespace scratchbird::core
