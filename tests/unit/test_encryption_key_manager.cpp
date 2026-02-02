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
#include "scratchbird/core/database.h"
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;

TEST(EncryptionKeyManagerTest, GenerateRotateDecrypt)
{
    const char *test_db = "test_encryption_key_manager.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK);

    EncryptionKeyManager *ekm = db.encryption_key_manager();
    ASSERT_NE(ekm, nullptr);

    std::vector<uint8_t> master_key(32, 0x42);
    status = ekm->setMasterKey(master_key, &ctx);
    ASSERT_EQ(status, Status::OK);

    ID domain_id = generateUuidV7();
    ID key_id;
    status = ekm->generateKey(domain_id, EncryptionAlgorithm::AES256_GCM, key_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    EncryptionKey active_key;
    status = ekm->getActiveKey(domain_id, active_key, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(active_key.key_version, 1u);
    ASSERT_EQ(active_key.algorithm, EncryptionAlgorithm::AES256_GCM);
    ASSERT_TRUE(active_key.active);

    std::vector<uint8_t> decrypted;
    status = ekm->decryptKey(active_key, decrypted, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(decrypted.size(), 32u);

    std::vector<uint8_t> decrypted_again;
    status = ekm->decryptKey(active_key, decrypted_again, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(decrypted_again, decrypted);

    status = ekm->rotateKey(domain_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    EncryptionKey rotated_key;
    status = ekm->getActiveKey(domain_id, rotated_key, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(rotated_key.key_version, 2u);
    ASSERT_NE(rotated_key.key_id, active_key.key_id);

    EncryptionKey old_key;
    status = ekm->getKeyByVersion(domain_id, 1, old_key, &ctx);
    ASSERT_EQ(status, Status::OK);

    std::vector<uint8_t> old_decrypted;
    status = ekm->decryptKey(old_key, old_decrypted, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(old_decrypted.size(), 32u);

    status = ekm->deleteKey(old_key.key_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    EncryptionKey missing_key;
    status = ekm->getKeyByVersion(domain_id, 1, missing_key, &ctx);
    ASSERT_EQ(status, Status::NOT_FOUND);

    db.close();
    std::remove(test_db);
}

TEST(EncryptionKeyManagerTest, PerDomainIsolation)
{
    const char *test_db = "test_encryption_key_manager_isolation.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK);

    EncryptionKeyManager *ekm = db.encryption_key_manager();
    ASSERT_NE(ekm, nullptr);

    std::vector<uint8_t> master_key(32, 0x11);
    status = ekm->setMasterKey(master_key, &ctx);
    ASSERT_EQ(status, Status::OK);

    ID domain_a = generateUuidV7();
    ID domain_b = generateUuidV7();
    ID key_a;
    ID key_b;

    status = ekm->generateKey(domain_a, EncryptionAlgorithm::AES128_GCM, key_a, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = ekm->generateKey(domain_b, EncryptionAlgorithm::AES256_GCM, key_b, &ctx);
    ASSERT_EQ(status, Status::OK);

    EncryptionKey active_a;
    EncryptionKey active_b;
    status = ekm->getActiveKey(domain_a, active_a, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = ekm->getActiveKey(domain_b, active_b, &ctx);
    ASSERT_EQ(status, Status::OK);

    ASSERT_NE(active_a.key_id, active_b.key_id);
    ASSERT_NE(active_a.domain_id, active_b.domain_id);

    db.close();
    std::remove(test_db);
}
