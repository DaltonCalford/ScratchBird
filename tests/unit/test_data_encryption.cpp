/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/data_encryption.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace scratchbird::core;

namespace
{
    uint8_t hexNibble(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return static_cast<uint8_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f')
        {
            return static_cast<uint8_t>(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F')
        {
            return static_cast<uint8_t>(c - 'A' + 10);
        }
        return 0;
    }

    std::vector<uint8_t> hexToBytes(const std::string &hex)
    {
        std::vector<uint8_t> out;
        if (hex.size() % 2 != 0)
        {
            return out;
        }
        out.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            uint8_t high = hexNibble(hex[i]);
            uint8_t low = hexNibble(hex[i + 1]);
            out.push_back(static_cast<uint8_t>((high << 4) | low));
        }
        return out;
    }
} // namespace

TEST(DataEncryptionTest, NistVectorAES128GCM)
{
    std::vector<uint8_t> key = hexToBytes("00000000000000000000000000000000");
    std::vector<uint8_t> expected_plaintext =
        hexToBytes("00000000000000000000000000000000");

    EncryptedValue encrypted;
    encrypted.algorithm = EncryptionAlgorithm::AES128_GCM;
    encrypted.iv = hexToBytes("000000000000000000000000");
    encrypted.ciphertext = hexToBytes("0388dace60b6a392f328c2b971b2fe78");
    encrypted.auth_tag = hexToBytes("ab6e47d42cec13bdf53a67b21257bddf");

    std::vector<uint8_t> plaintext;
    ErrorContext ctx;
    Status status = DataEncryption::decrypt(encrypted, key, plaintext, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(plaintext, expected_plaintext);
}

TEST(DataEncryptionTest, NistVectorAES256GCM)
{
    std::vector<uint8_t> key = hexToBytes(
        "0000000000000000000000000000000000000000000000000000000000000000");
    std::vector<uint8_t> expected_plaintext =
        hexToBytes("00000000000000000000000000000000");

    EncryptedValue encrypted;
    encrypted.algorithm = EncryptionAlgorithm::AES256_GCM;
    encrypted.iv = hexToBytes("000000000000000000000000");
    encrypted.ciphertext = hexToBytes("cea7403d4d606b6e074ec5d3baf39d18");
    encrypted.auth_tag = hexToBytes("d0d1c8a799996bf0265b98b5d48ab919");

    std::vector<uint8_t> plaintext;
    ErrorContext ctx;
    Status status = DataEncryption::decrypt(encrypted, key, plaintext, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(plaintext, expected_plaintext);
}

TEST(DataEncryptionTest, EncryptDecryptRoundTripAES128)
{
    std::vector<uint8_t> key(16, 0x11);
    std::vector<uint8_t> plaintext = {
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c};

    EncryptedValue encrypted;
    ErrorContext ctx;
    Status status = DataEncryption::encrypt(plaintext, key,
                                            EncryptionAlgorithm::AES128_GCM,
                                            encrypted, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(encrypted.algorithm, EncryptionAlgorithm::AES128_GCM);
    ASSERT_EQ(encrypted.iv.size(), 12u);
    ASSERT_EQ(encrypted.auth_tag.size(), 16u);

    std::vector<uint8_t> decrypted;
    status = DataEncryption::decrypt(encrypted, key, decrypted, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(decrypted, plaintext);
}

TEST(DataEncryptionTest, EncryptDecryptRoundTripAES256)
{
    std::vector<uint8_t> key(32, 0x22);
    std::vector<uint8_t> plaintext(64, 0x5a);

    EncryptedValue encrypted;
    ErrorContext ctx;
    Status status = DataEncryption::encrypt(plaintext, key,
                                            EncryptionAlgorithm::AES256_GCM,
                                            encrypted, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(encrypted.algorithm, EncryptionAlgorithm::AES256_GCM);
    ASSERT_EQ(encrypted.iv.size(), 12u);
    ASSERT_EQ(encrypted.auth_tag.size(), 16u);

    std::vector<uint8_t> decrypted;
    status = DataEncryption::decrypt(encrypted, key, decrypted, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(decrypted, plaintext);
}

TEST(DataEncryptionTest, AuthenticationTagValidation)
{
    std::vector<uint8_t> key(16, 0x33);
    std::vector<uint8_t> plaintext(32, 0x44);

    EncryptedValue encrypted;
    ErrorContext ctx;
    Status status = DataEncryption::encrypt(plaintext, key,
                                            EncryptionAlgorithm::AES128_GCM,
                                            encrypted, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    encrypted.auth_tag[0] ^= 0x01;

    std::vector<uint8_t> decrypted;
    status = DataEncryption::decrypt(encrypted, key, decrypted, &ctx);
    EXPECT_EQ(status, Status::DATA_CORRUPTED);
}

TEST(DataEncryptionTest, GenerateIVLength)
{
    std::vector<uint8_t> iv;
    DataEncryption::generateIV(iv);
    EXPECT_EQ(iv.size(), 12u);

    std::vector<uint8_t> iv_second;
    DataEncryption::generateIV(iv_second);
    EXPECT_EQ(iv_second.size(), 12u);
    EXPECT_NE(iv, iv_second);
}

TEST(DataEncryptionTest, PerformanceSmallValues)
{
    constexpr int kIterations = 20000;
    std::vector<uint8_t> key(32, 0x55);
    std::vector<uint8_t> plaintext(32, 0x66);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i)
    {
        EncryptedValue encrypted;
        Status status = DataEncryption::encrypt(plaintext, key,
                                                EncryptionAlgorithm::AES256_GCM,
                                                encrypted, nullptr);
        ASSERT_EQ(status, Status::OK);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    if (duration.count() == 0)
    {
        GTEST_SKIP() << "Timer resolution too coarse";
    }

    double ops_per_sec = (kIterations * 1000000.0) / duration.count();
    std::cout << "AES-GCM encryption throughput: "
              << static_cast<int>(ops_per_sec) << " ops/sec\n";
    EXPECT_GT(ops_per_sec, 10000.0);
}

TEST(DataEncryptionTest, ConcurrentEncryptDecrypt)
{
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;
    std::vector<uint8_t> key(32, 0x42);
    std::vector<uint8_t> plaintext(128, 0x5a);

    std::atomic<bool> failed{false};
    std::mutex error_mutex;
    std::string error_msg;

    auto worker = [&]() {
        for (int i = 0; i < kIterations && !failed.load(); ++i)
        {
            EncryptedValue encrypted;
            ErrorContext ctx;
            Status status = DataEncryption::encrypt(plaintext, key,
                                                    EncryptionAlgorithm::AES256_GCM,
                                                    encrypted, &ctx);
            if (status != Status::OK)
            {
                std::lock_guard<std::mutex> lock(error_mutex);
                failed = true;
                error_msg = "Encrypt failed: " + ctx.message;
                return;
            }

            std::vector<uint8_t> decrypted;
            status = DataEncryption::decrypt(encrypted, key, decrypted, &ctx);
            if (status != Status::OK || decrypted != plaintext)
            {
                std::lock_guard<std::mutex> lock(error_mutex);
                failed = true;
                error_msg = "Decrypt failed or mismatch";
                return;
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_FALSE(failed) << error_msg;
}
