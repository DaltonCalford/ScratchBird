#include <gtest/gtest.h>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"

using namespace scratchbird::core;

class ConstraintsCRUDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: This is a unit test demonstrating the constraints CRUD API
        // Full integration testing would require a complete CatalogManager setup
    }

    void TearDown() override {}
};

// Test ConstraintType enum
TEST_F(ConstraintsCRUDTest, ConstraintTypes) {
    EXPECT_EQ(static_cast<uint8_t>(ConstraintType::PRIMARY_KEY), 0);
    EXPECT_EQ(static_cast<uint8_t>(ConstraintType::UNIQUE), 1);
    EXPECT_EQ(static_cast<uint8_t>(ConstraintType::CHECK), 2);
    EXPECT_EQ(static_cast<uint8_t>(ConstraintType::FOREIGN_KEY), 3);
    EXPECT_EQ(static_cast<uint8_t>(ConstraintType::NOT_NULL), 4);
    EXPECT_EQ(static_cast<uint8_t>(ConstraintType::EXCLUSION), 5);
}

// Test ConstraintInfo structure
TEST_F(ConstraintsCRUDTest, ConstraintInfoDefaults) {
    ConstraintInfo constraint;

    EXPECT_EQ(constraint.constraint_id, ID{});
    EXPECT_TRUE(constraint.constraint_name.empty());
    EXPECT_EQ(constraint.table_id, ID{});
    EXPECT_TRUE(constraint.column_names.empty());
    EXPECT_TRUE(constraint.check_expression.empty());
    EXPECT_EQ(constraint.check_expr_oid, 0);
    EXPECT_EQ(constraint.referenced_table_id, ID{});
    EXPECT_TRUE(constraint.referenced_columns.empty());
    EXPECT_EQ(constraint.on_delete, FKAction::NO_ACTION);
    EXPECT_EQ(constraint.on_update, FKAction::NO_ACTION);
    EXPECT_EQ(constraint.match_type, FKMatchType::SIMPLE);
    EXPECT_FALSE(constraint.is_deferrable);
    EXPECT_FALSE(constraint.initially_deferred);
    EXPECT_TRUE(constraint.is_enabled);
    EXPECT_TRUE(constraint.is_validated);
    EXPECT_FALSE(constraint.is_system_generated);
    EXPECT_EQ(constraint.owner_id, ID{});
    EXPECT_EQ(constraint.created_time, 0);
    EXPECT_EQ(constraint.validated_time, 0);
}

// Test PRIMARY KEY constraint creation
TEST_F(ConstraintsCRUDTest, PrimaryKeyConstraint) {
    ConstraintInfo pk_constraint;
    pk_constraint.constraint_name = "pk_users_id";
    pk_constraint.constraint_type = ConstraintType::PRIMARY_KEY;
    pk_constraint.column_names = {"id"};
    pk_constraint.is_deferrable = false;
    pk_constraint.is_enabled = true;

    EXPECT_EQ(pk_constraint.constraint_type, ConstraintType::PRIMARY_KEY);
    EXPECT_EQ(pk_constraint.column_names.size(), 1);
    EXPECT_EQ(pk_constraint.column_names[0], "id");
    EXPECT_FALSE(pk_constraint.is_deferrable);
    EXPECT_TRUE(pk_constraint.is_enabled);
}

// Test UNIQUE constraint creation
TEST_F(ConstraintsCRUDTest, UniqueConstraint) {
    ConstraintInfo unique_constraint;
    unique_constraint.constraint_name = "uq_users_email";
    unique_constraint.constraint_type = ConstraintType::UNIQUE;
    unique_constraint.column_names = {"email"};

    EXPECT_EQ(unique_constraint.constraint_type, ConstraintType::UNIQUE);
    EXPECT_EQ(unique_constraint.column_names.size(), 1);
    EXPECT_EQ(unique_constraint.column_names[0], "email");
}

// Test CHECK constraint creation
TEST_F(ConstraintsCRUDTest, CheckConstraint) {
    ConstraintInfo check_constraint;
    check_constraint.constraint_name = "chk_users_age";
    check_constraint.constraint_type = ConstraintType::CHECK;
    check_constraint.column_names = {"age"};
    check_constraint.check_expression = "age >= 0 AND age <= 120";

    EXPECT_EQ(check_constraint.constraint_type, ConstraintType::CHECK);
    EXPECT_EQ(check_constraint.check_expression, "age >= 0 AND age <= 120");
    EXPECT_EQ(check_constraint.column_names.size(), 1);
}

// Test FOREIGN KEY constraint creation
TEST_F(ConstraintsCRUDTest, ForeignKeyConstraint) {
    ConstraintInfo fk_constraint;
    fk_constraint.constraint_name = "fk_orders_user_id";
    fk_constraint.constraint_type = ConstraintType::FOREIGN_KEY;
    fk_constraint.column_names = {"user_id"};
    fk_constraint.referenced_columns = {"id"};
    fk_constraint.on_delete = FKAction::CASCADE;
    fk_constraint.on_update = FKAction::RESTRICT;
    fk_constraint.match_type = FKMatchType::SIMPLE;

    EXPECT_EQ(fk_constraint.constraint_type, ConstraintType::FOREIGN_KEY);
    EXPECT_EQ(fk_constraint.on_delete, FKAction::CASCADE);
    EXPECT_EQ(fk_constraint.on_update, FKAction::RESTRICT);
    EXPECT_EQ(fk_constraint.match_type, FKMatchType::SIMPLE);
}

// Test NOT NULL constraint creation
TEST_F(ConstraintsCRUDTest, NotNullConstraint) {
    ConstraintInfo nn_constraint;
    nn_constraint.constraint_name = "nn_users_username";
    nn_constraint.constraint_type = ConstraintType::NOT_NULL;
    nn_constraint.column_names = {"username"};

    EXPECT_EQ(nn_constraint.constraint_type, ConstraintType::NOT_NULL);
    EXPECT_EQ(nn_constraint.column_names.size(), 1);
    EXPECT_EQ(nn_constraint.column_names[0], "username");
}

// Test composite PRIMARY KEY
TEST_F(ConstraintsCRUDTest, CompositePrimaryKey) {
    ConstraintInfo pk_constraint;
    pk_constraint.constraint_name = "pk_order_items";
    pk_constraint.constraint_type = ConstraintType::PRIMARY_KEY;
    pk_constraint.column_names = {"order_id", "item_id"};

    EXPECT_EQ(pk_constraint.constraint_type, ConstraintType::PRIMARY_KEY);
    EXPECT_EQ(pk_constraint.column_names.size(), 2);
    EXPECT_EQ(pk_constraint.column_names[0], "order_id");
    EXPECT_EQ(pk_constraint.column_names[1], "item_id");
}

// Test composite FOREIGN KEY
TEST_F(ConstraintsCRUDTest, CompositeForeignKey) {
    ConstraintInfo fk_constraint;
    fk_constraint.constraint_name = "fk_line_items_order";
    fk_constraint.constraint_type = ConstraintType::FOREIGN_KEY;
    fk_constraint.column_names = {"order_id", "item_id"};
    fk_constraint.referenced_columns = {"id", "item_id"};
    fk_constraint.on_delete = FKAction::CASCADE;

    EXPECT_EQ(fk_constraint.column_names.size(), 2);
    EXPECT_EQ(fk_constraint.referenced_columns.size(), 2);
}

// Test deferrable constraint
TEST_F(ConstraintsCRUDTest, DeferrableConstraint) {
    ConstraintInfo fk_constraint;
    fk_constraint.constraint_name = "fk_deferrable";
    fk_constraint.constraint_type = ConstraintType::FOREIGN_KEY;
    fk_constraint.is_deferrable = true;
    fk_constraint.initially_deferred = true;

    EXPECT_TRUE(fk_constraint.is_deferrable);
    EXPECT_TRUE(fk_constraint.initially_deferred);
}

// Test constraint enable/disable
TEST_F(ConstraintsCRUDTest, ConstraintEnabledState) {
    ConstraintInfo constraint;
    constraint.constraint_name = "test_constraint";
    constraint.is_enabled = true;

    EXPECT_TRUE(constraint.is_enabled);

    constraint.is_enabled = false;
    EXPECT_FALSE(constraint.is_enabled);
}

// Test constraint validation state
TEST_F(ConstraintsCRUDTest, ConstraintValidationState) {
    ConstraintInfo constraint;
    constraint.constraint_name = "test_constraint";
    constraint.is_validated = true;
    constraint.validated_time = 1234567890;

    EXPECT_TRUE(constraint.is_validated);
    EXPECT_EQ(constraint.validated_time, 1234567890);
}

// Test system-generated constraint name
TEST_F(ConstraintsCRUDTest, SystemGeneratedName) {
    ConstraintInfo constraint;
    constraint.constraint_name = "SYS_C001234";
    constraint.is_system_generated = true;

    EXPECT_TRUE(constraint.is_system_generated);
    EXPECT_EQ(constraint.constraint_name, "SYS_C001234");
}

// Test EXCLUSION constraint (PostgreSQL extension)
TEST_F(ConstraintsCRUDTest, ExclusionConstraint) {
    ConstraintInfo excl_constraint;
    excl_constraint.constraint_name = "excl_room_time_overlap";
    excl_constraint.constraint_type = ConstraintType::EXCLUSION;
    excl_constraint.column_names = {"room_id", "time_range"};
    excl_constraint.exclusion_operator = "&&";
    excl_constraint.index_method = "GIST";

    EXPECT_EQ(excl_constraint.constraint_type, ConstraintType::EXCLUSION);
    EXPECT_EQ(excl_constraint.exclusion_operator, "&&");
    EXPECT_EQ(excl_constraint.index_method, "GIST");
}

// Test FK action combinations
TEST_F(ConstraintsCRUDTest, ForeignKeyActions) {
    // Test CASCADE/CASCADE
    ConstraintInfo fk1;
    fk1.constraint_type = ConstraintType::FOREIGN_KEY;
    fk1.on_delete = FKAction::CASCADE;
    fk1.on_update = FKAction::CASCADE;
    EXPECT_EQ(fk1.on_delete, FKAction::CASCADE);
    EXPECT_EQ(fk1.on_update, FKAction::CASCADE);

    // Test SET NULL/RESTRICT
    ConstraintInfo fk2;
    fk2.constraint_type = ConstraintType::FOREIGN_KEY;
    fk2.on_delete = FKAction::SET_NULL;
    fk2.on_update = FKAction::RESTRICT;
    EXPECT_EQ(fk2.on_delete, FKAction::SET_NULL);
    EXPECT_EQ(fk2.on_update, FKAction::RESTRICT);

    // Test SET DEFAULT/NO ACTION
    ConstraintInfo fk3;
    fk3.constraint_type = ConstraintType::FOREIGN_KEY;
    fk3.on_delete = FKAction::SET_DEFAULT;
    fk3.on_update = FKAction::NO_ACTION;
    EXPECT_EQ(fk3.on_delete, FKAction::SET_DEFAULT);
    EXPECT_EQ(fk3.on_update, FKAction::NO_ACTION);
}

// Test FK match types
TEST_F(ConstraintsCRUDTest, ForeignKeyMatchTypes) {
    // SIMPLE match (default)
    ConstraintInfo fk1;
    fk1.constraint_type = ConstraintType::FOREIGN_KEY;
    fk1.match_type = FKMatchType::SIMPLE;
    EXPECT_EQ(fk1.match_type, FKMatchType::SIMPLE);

    // FULL match
    ConstraintInfo fk2;
    fk2.constraint_type = ConstraintType::FOREIGN_KEY;
    fk2.match_type = FKMatchType::FULL;
    EXPECT_EQ(fk2.match_type, FKMatchType::FULL);

    // PARTIAL match (reserved)
    ConstraintInfo fk3;
    fk3.constraint_type = ConstraintType::FOREIGN_KEY;
    fk3.match_type = FKMatchType::PARTIAL;
    EXPECT_EQ(fk3.match_type, FKMatchType::PARTIAL);
}
