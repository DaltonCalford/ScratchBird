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

#include <string>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/security/view_security.h"

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::security::ViewSecurityManager;
using scratchbird::security::ViewSecurityMode;
using scratchbird::security::ViewSecurityOptions;

namespace {

class ViewSecurityContractTest : public ::testing::Test {
protected:
    ViewSecurityManager& manager_ = ViewSecurityManager::getInstance();
    std::vector<uint32_t> registered_views_;

    void TearDown() override {
        manager_.currentStack().clear();
        for (uint32_t view_id : registered_views_) {
            manager_.unregisterView(view_id);
        }
        registered_views_.clear();
    }

    void registerView(uint32_t view_id, const ViewSecurityOptions& options) {
        ErrorContext ctx;
        ASSERT_EQ(manager_.registerView(view_id, options, &ctx), Status::OK) << ctx.message;
        registered_views_.push_back(view_id);
    }
};

}  // namespace

TEST_F(ViewSecurityContractTest, InvokerContextPassesThroughCallerScopedChecks) {
    registerView(101, ViewSecurityOptions::defaultOptions(42));

    const auto ctx = manager_.enterView(101, 7);
    EXPECT_EQ(ctx.options().mode, ViewSecurityMode::INVOKER);
    EXPECT_EQ(manager_.currentStack().effectiveUserId(), 7u);

    ErrorContext err;
    EXPECT_EQ(manager_.checkTableAccess(77, 1, &err), Status::OK) << err.message;
    EXPECT_EQ(manager_.checkColumnAccess(77, 3, 1, &err), Status::OK) << err.message;

    manager_.exitView(101);
}

TEST_F(ViewSecurityContractTest, DefinerContextFailsClosedWithoutPermissionBackend) {
    registerView(102, ViewSecurityOptions::definerOptions(42));

    const auto ctx = manager_.enterView(102, 7);
    EXPECT_EQ(ctx.options().mode, ViewSecurityMode::DEFINER);
    EXPECT_EQ(manager_.currentStack().effectiveUserId(), 42u);

    ErrorContext table_err;
    EXPECT_EQ(manager_.checkTableAccess(88, 1, &table_err), Status::PERMISSION_DENIED);
    EXPECT_NE(table_err.message.find("integrated permission backend"), std::string::npos);

    ErrorContext column_err;
    EXPECT_EQ(manager_.checkColumnAccess(88, 4, 1, &column_err), Status::PERMISSION_DENIED);
    EXPECT_NE(column_err.message.find("integrated permission backend"), std::string::npos);

    manager_.exitView(102);
}

TEST_F(ViewSecurityContractTest, CheckOptionRequiresRowDataAndFailsClosedWithoutEvaluator) {
    ViewSecurityOptions options = ViewSecurityOptions::defaultOptions(11);
    options.check_option = true;
    options.local_check_only = true;
    registerView(103, options);

    ErrorContext null_err;
    EXPECT_EQ(manager_.validateCheckOption(103, nullptr, &null_err), Status::INVALID_ARGUMENT);
    EXPECT_NE(null_err.message.find("requires row data"), std::string::npos);

    const int row_marker = 1;
    ErrorContext eval_err;
    EXPECT_EQ(manager_.validateCheckOption(103, &row_marker, &eval_err), Status::PERMISSION_DENIED);
    EXPECT_NE(eval_err.message.find("integrated predicate evaluation backend"), std::string::npos);
    EXPECT_EQ(manager_.getCheckOptionMode(103), ViewSecurityManager::CheckOptionMode::LOCAL);
}

TEST_F(ViewSecurityContractTest, SecurityBarrierAndNestedEffectiveUserResolutionStayDeterministic) {
    registerView(201, ViewSecurityOptions::defaultOptions(12));
    registerView(202, ViewSecurityOptions::barrierOptions(99));

    manager_.enterView(201, 7);
    EXPECT_EQ(manager_.currentStack().effectiveUserId(), 7u);
    EXPECT_FALSE(manager_.currentStack().hasSecurityBarrier());

    manager_.enterView(202, 7);
    EXPECT_EQ(manager_.currentStack().effectiveUserId(), 99u);
    EXPECT_TRUE(manager_.currentStack().hasSecurityBarrier());
    EXPECT_FALSE(manager_.canPushPredicate(202));

    manager_.exitView(202);
    manager_.exitView(201);
}
