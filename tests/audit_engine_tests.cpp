#include "scratchbird/audit/audit_engine.h"

#include <gtest/gtest.h>

using scratchbird::audit::AuditEngine;
using scratchbird::audit::AuditEventKind;
using scratchbird::audit::AuditPolicy;

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(AuditEngineTests, PolicyEvaluation)
{
    auto& ae = AuditEngine::instance();
    ae.clear();
    AuditPolicy p;
    p.name = "default";
    p.ddl = true;
    p.dml = false;
    p.select = false;
    p.admin = true;
    ae.set_policy(p);
    ae.record(AuditEventKind::DDL, "u", "t", "CREATE TABLE", "");
    ae.record(AuditEventKind::DML, "u", "t", "INSERT", "");
    ae.record(AuditEventKind::Admin, "admin", "db", "CHECKPOINT", "");
    auto events = ae.recent(10);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].operation, "CREATE TABLE");
    EXPECT_EQ(events[1].operation, "CHECKPOINT");
}
