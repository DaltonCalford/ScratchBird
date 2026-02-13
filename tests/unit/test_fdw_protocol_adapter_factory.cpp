/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "gtest/gtest.h"

#include "scratchbird/fdw/protocol_adapter.h"

#include <set>

namespace scratchbird::fdw {
namespace {

TEST(FDWProtocolAdapterFactoryTest, CreateImplementedAdapters) {
    auto pg = ProtocolAdapterFactory::create(RemoteDatabaseType::POSTGRESQL);
    ASSERT_NE(pg, nullptr);
    EXPECT_EQ(pg->getDatabaseType(), RemoteDatabaseType::POSTGRESQL);

    auto mysql = ProtocolAdapterFactory::create(RemoteDatabaseType::MYSQL);
    ASSERT_NE(mysql, nullptr);
    EXPECT_EQ(mysql->getDatabaseType(), RemoteDatabaseType::MYSQL);

    auto firebird = ProtocolAdapterFactory::create(RemoteDatabaseType::FIREBIRD);
    ASSERT_NE(firebird, nullptr);
    EXPECT_EQ(firebird->getDatabaseType(), RemoteDatabaseType::FIREBIRD);
}

TEST(FDWProtocolAdapterFactoryTest, UnsupportedTypesReturnNull) {
    EXPECT_EQ(ProtocolAdapterFactory::create(RemoteDatabaseType::MSSQL), nullptr);
    EXPECT_EQ(ProtocolAdapterFactory::create(RemoteDatabaseType::SCRATCHBIRD), nullptr);
    EXPECT_EQ(ProtocolAdapterFactory::create(RemoteDatabaseType::ORACLE), nullptr);
    EXPECT_EQ(ProtocolAdapterFactory::create(RemoteDatabaseType::SQLITE), nullptr);
    EXPECT_EQ(ProtocolAdapterFactory::create(RemoteDatabaseType::ODBC), nullptr);
    EXPECT_EQ(ProtocolAdapterFactory::create(RemoteDatabaseType::JDBC), nullptr);
}

TEST(FDWProtocolAdapterFactoryTest, SupportedTypesMatchesFactoryBehavior) {
    const auto supported = ProtocolAdapterFactory::supportedTypes();
    const std::set<RemoteDatabaseType> supported_set(supported.begin(), supported.end());

    for (RemoteDatabaseType type : {
             RemoteDatabaseType::POSTGRESQL,
             RemoteDatabaseType::MYSQL,
             RemoteDatabaseType::MSSQL,
             RemoteDatabaseType::FIREBIRD,
             RemoteDatabaseType::SCRATCHBIRD,
             RemoteDatabaseType::ORACLE,
             RemoteDatabaseType::SQLITE,
             RemoteDatabaseType::ODBC,
             RemoteDatabaseType::JDBC}) {
        const bool advertised = ProtocolAdapterFactory::isSupported(type);
        const bool listed = supported_set.find(type) != supported_set.end();
        auto created = ProtocolAdapterFactory::create(type);
        const bool creatable = (created != nullptr);

        EXPECT_EQ(advertised, listed);
        EXPECT_EQ(advertised, creatable);
    }
}

}  // namespace
}  // namespace scratchbird::fdw
