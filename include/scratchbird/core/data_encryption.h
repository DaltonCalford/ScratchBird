#pragma once

#include <cstdint>
#include <vector>
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

namespace scratchbird::core
{
    struct EncryptedValue
    {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> iv;
        std::vector<uint8_t> auth_tag;
        uint32_t key_version = 0;
        EncryptionAlgorithm algorithm = EncryptionAlgorithm::NONE;
    };

    class DataEncryption
    {
    public:
        static Status encrypt(const std::vector<uint8_t> &plaintext,
                              const std::vector<uint8_t> &key,
                              EncryptionAlgorithm algorithm,
                              EncryptedValue &encrypted_out,
                              ErrorContext *ctx = nullptr);

        static Status decrypt(const EncryptedValue &encrypted,
                              const std::vector<uint8_t> &key,
                              std::vector<uint8_t> &plaintext_out,
                              ErrorContext *ctx = nullptr);

        static void generateIV(std::vector<uint8_t> &iv_out);

        static bool hasHardwareAcceleration();

    private:
        static Status encryptAES256GCM(const std::vector<uint8_t> &plaintext,
                                       const std::vector<uint8_t> &key,
                                       EncryptedValue &encrypted_out,
                                       ErrorContext *ctx);

        static Status encryptAES128GCM(const std::vector<uint8_t> &plaintext,
                                       const std::vector<uint8_t> &key,
                                       EncryptedValue &encrypted_out,
                                       ErrorContext *ctx);

        static Status decryptAES256GCM(const EncryptedValue &encrypted,
                                       const std::vector<uint8_t> &key,
                                       std::vector<uint8_t> &plaintext_out,
                                       ErrorContext *ctx);

        static Status decryptAES128GCM(const EncryptedValue &encrypted,
                                       const std::vector<uint8_t> &key,
                                       std::vector<uint8_t> &plaintext_out,
                                       ErrorContext *ctx);
    };
} // namespace scratchbird::core
