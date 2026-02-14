/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/domain_validation.h"

using namespace scratchbird::core;

TEST(DomainValidationFrozenTest, AllowsWholeValueReplacement)
{
    DomainInfo domain;
    domain.domain_name = "[sb_cas_dom]frozen";
    domain.dialect_tag = "cassandra";
    domain.compat_name = "frozen";

    ErrorContext ctx;
    Status status = DomainValidation::validateCollectionMutation(
        domain, CollectionMutationKind::REPLACE_VALUE, &ctx);
    EXPECT_EQ(status, Status::OK);
}

TEST(DomainValidationFrozenTest, RejectsElementMutation)
{
    DomainInfo domain;
    domain.domain_name = "[sb_cas_dom]frozen";
    domain.dialect_tag = "cassandra";
    domain.compat_name = "frozen";

    ErrorContext ctx;
    Status status = DomainValidation::validateCollectionMutation(
        domain, CollectionMutationKind::UPDATE_ELEMENT, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(ctx.message.find("whole-value replacement only"), std::string::npos);
}

TEST(DomainValidationFrozenTest, NonFrozenCollectionsAllowElementMutation)
{
    DomainInfo domain;
    domain.domain_name = "[sb_cas_dom]list";
    domain.dialect_tag = "cassandra";
    domain.compat_name = "list";

    ErrorContext ctx;
    Status status = DomainValidation::validateCollectionMutation(
        domain, CollectionMutationKind::UPDATE_ELEMENT, &ctx);
    EXPECT_EQ(status, Status::OK);
}
