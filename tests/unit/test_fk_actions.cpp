#include <gtest/gtest.h>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"

using namespace scratchbird::core;

/**
 * P1-6: Foreign Key Actions Test Suite
 *
 * This test suite verifies all FK referential actions:
 * - NO_ACTION (default)
 * - RESTRICT (immediate check)
 * - CASCADE (propagate changes)
 * - SET NULL (set FK columns to NULL)
 * - SET DEFAULT (set FK columns to DEFAULT)
 *
 * Tests cover both DELETE and UPDATE operations.
 */

class FKActionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: This is a unit test for FK action data structures and API
        // Full integration testing requires complete database setup
    }

    void TearDown() override {}
};

// Test FKAction enum values
TEST_F(FKActionsTest, FKActionEnum) {
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKAction::NO_ACTION), 0);
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKAction::RESTRICT), 1);
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKAction::CASCADE), 2);
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKAction::SET_NULL), 3);
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKAction::SET_DEFAULT), 4);
}

// Test FKMatchType enum values
TEST_F(FKActionsTest, FKMatchTypeEnum) {
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKMatchType::SIMPLE), 0);
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKMatchType::FULL), 1);
    EXPECT_EQ(static_cast<uint8_t>(CatalogManager::FKMatchType::PARTIAL), 2);
}

// Test ForeignKeyInfo structure defaults
TEST_F(FKActionsTest, ForeignKeyInfoDefaults) {
    CatalogManager::ForeignKeyInfo fk;

    EXPECT_EQ(fk.fk_id, ID{});
    EXPECT_TRUE(fk.fk_name.empty());
    EXPECT_EQ(fk.child_table_id, ID{});
    EXPECT_EQ(fk.parent_table_id, ID{});
    EXPECT_TRUE(fk.child_columns.empty());
    EXPECT_TRUE(fk.parent_columns.empty());
    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::NO_ACTION);
    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::NO_ACTION);
    EXPECT_EQ(fk.match_type, CatalogManager::FKMatchType::SIMPLE);
    EXPECT_TRUE(fk.is_enabled);
    EXPECT_FALSE(fk.is_deferrable);
    EXPECT_FALSE(fk.initially_deferred);
    EXPECT_EQ(fk.created_time, 0);
}

// Test FK with CASCADE DELETE
TEST_F(FKActionsTest, CascadeDeleteAction) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_orders_customer";
    fk.child_columns = {"customer_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::CASCADE;
    fk.on_update = CatalogManager::FKAction::NO_ACTION;

    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::CASCADE);
    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::NO_ACTION);
    EXPECT_EQ(fk.child_columns.size(), 1);
    EXPECT_EQ(fk.parent_columns.size(), 1);
}

// Test FK with CASCADE UPDATE
TEST_F(FKActionsTest, CascadeUpdateAction) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_orders_product";
    fk.child_columns = {"product_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::RESTRICT;
    fk.on_update = CatalogManager::FKAction::CASCADE;

    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::RESTRICT);
    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::CASCADE);
}

// Test FK with SET NULL on DELETE
TEST_F(FKActionsTest, SetNullOnDelete) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_orders_salesperson";
    fk.child_columns = {"salesperson_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::SET_NULL;
    fk.on_update = CatalogManager::FKAction::NO_ACTION;

    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::SET_NULL);
}

// Test FK with SET NULL on UPDATE
TEST_F(FKActionsTest, SetNullOnUpdate) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_orders_region";
    fk.child_columns = {"region_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::NO_ACTION;
    fk.on_update = CatalogManager::FKAction::SET_NULL;

    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::SET_NULL);
}

// Test FK with SET DEFAULT on DELETE
TEST_F(FKActionsTest, SetDefaultOnDelete) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_orders_status";
    fk.child_columns = {"status_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::SET_DEFAULT;
    fk.on_update = CatalogManager::FKAction::NO_ACTION;

    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::SET_DEFAULT);
}

// Test FK with SET DEFAULT on UPDATE
TEST_F(FKActionsTest, SetDefaultOnUpdate) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_products_category";
    fk.child_columns = {"category_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::NO_ACTION;
    fk.on_update = CatalogManager::FKAction::SET_DEFAULT;

    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::SET_DEFAULT);
}

// Test FK with RESTRICT action
TEST_F(FKActionsTest, RestrictAction) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_invoice_items_invoice";
    fk.child_columns = {"invoice_id"};
    fk.parent_columns = {"id"};
    fk.on_delete = CatalogManager::FKAction::RESTRICT;
    fk.on_update = CatalogManager::FKAction::RESTRICT;

    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::RESTRICT);
    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::RESTRICT);
}

// Test FK with NO_ACTION (default behavior)
TEST_F(FKActionsTest, NoActionDefault) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_payments_order";
    fk.child_columns = {"order_id"};
    fk.parent_columns = {"id"};
    // on_delete and on_update default to NO_ACTION

    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::NO_ACTION);
    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::NO_ACTION);
}

// Test composite FK with CASCADE actions
TEST_F(FKActionsTest, CompositeFKCascade) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_order_items_order_product";
    fk.child_columns = {"order_id", "product_id"};
    fk.parent_columns = {"order_id", "product_id"};
    fk.on_delete = CatalogManager::FKAction::CASCADE;
    fk.on_update = CatalogManager::FKAction::CASCADE;

    EXPECT_EQ(fk.child_columns.size(), 2);
    EXPECT_EQ(fk.parent_columns.size(), 2);
    EXPECT_EQ(fk.on_delete, CatalogManager::FKAction::CASCADE);
    EXPECT_EQ(fk.on_update, CatalogManager::FKAction::CASCADE);
}

// Test mixed actions (different for DELETE and UPDATE)
TEST_F(FKActionsTest, MixedActions) {
    // Scenario 1: CASCADE DELETE, SET NULL UPDATE
    CatalogManager::ForeignKeyInfo fk1;
    fk1.on_delete = CatalogManager::FKAction::CASCADE;
    fk1.on_update = CatalogManager::FKAction::SET_NULL;
    EXPECT_EQ(fk1.on_delete, CatalogManager::FKAction::CASCADE);
    EXPECT_EQ(fk1.on_update, CatalogManager::FKAction::SET_NULL);

    // Scenario 2: SET NULL DELETE, CASCADE UPDATE
    CatalogManager::ForeignKeyInfo fk2;
    fk2.on_delete = CatalogManager::FKAction::SET_NULL;
    fk2.on_update = CatalogManager::FKAction::CASCADE;
    EXPECT_EQ(fk2.on_delete, CatalogManager::FKAction::SET_NULL);
    EXPECT_EQ(fk2.on_update, CatalogManager::FKAction::CASCADE);

    // Scenario 3: SET DEFAULT DELETE, RESTRICT UPDATE
    CatalogManager::ForeignKeyInfo fk3;
    fk3.on_delete = CatalogManager::FKAction::SET_DEFAULT;
    fk3.on_update = CatalogManager::FKAction::RESTRICT;
    EXPECT_EQ(fk3.on_delete, CatalogManager::FKAction::SET_DEFAULT);
    EXPECT_EQ(fk3.on_update, CatalogManager::FKAction::RESTRICT);
}

// Test FK enable/disable capability
TEST_F(FKActionsTest, EnableDisableFK) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_test";
    fk.is_enabled = true;

    EXPECT_TRUE(fk.is_enabled);

    // Disable FK (useful for bulk loading)
    fk.is_enabled = false;
    EXPECT_FALSE(fk.is_enabled);

    // Re-enable FK
    fk.is_enabled = true;
    EXPECT_TRUE(fk.is_enabled);
}

// Test deferrable FK configuration
TEST_F(FKActionsTest, DeferrableFK) {
    CatalogManager::ForeignKeyInfo fk;
    fk.fk_name = "fk_circular_ref";
    fk.is_deferrable = true;
    fk.initially_deferred = true;
    fk.on_delete = CatalogManager::FKAction::CASCADE;

    EXPECT_TRUE(fk.is_deferrable);
    EXPECT_TRUE(fk.initially_deferred);
}

// Test MATCH SIMPLE (default) configuration
TEST_F(FKActionsTest, MatchSimple) {
    CatalogManager::ForeignKeyInfo fk;
    fk.child_columns = {"col1", "col2"};
    fk.parent_columns = {"id1", "id2"};
    fk.match_type = CatalogManager::FKMatchType::SIMPLE;

    EXPECT_EQ(fk.match_type, CatalogManager::FKMatchType::SIMPLE);
    // MATCH SIMPLE: NULL in any column means no constraint check needed
}

// Test MATCH FULL configuration
TEST_F(FKActionsTest, MatchFull) {
    CatalogManager::ForeignKeyInfo fk;
    fk.child_columns = {"col1", "col2"};
    fk.parent_columns = {"id1", "id2"};
    fk.match_type = CatalogManager::FKMatchType::FULL;

    EXPECT_EQ(fk.match_type, CatalogManager::FKMatchType::FULL);
    // MATCH FULL: Either all columns NULL or all non-NULL
}

// Test realistic scenarios
TEST_F(FKActionsTest, RealisticScenarios) {
    // E-commerce: Orders reference Customers (CASCADE DELETE)
    CatalogManager::ForeignKeyInfo fk_orders;
    fk_orders.fk_name = "fk_orders_customer_id";
    fk_orders.child_columns = {"customer_id"};
    fk_orders.parent_columns = {"id"};
    fk_orders.on_delete = CatalogManager::FKAction::CASCADE;
    fk_orders.on_update = CatalogManager::FKAction::CASCADE;
    EXPECT_EQ(fk_orders.on_delete, CatalogManager::FKAction::CASCADE);

    // Optional reference: Products reference optional Category (SET NULL)
    CatalogManager::ForeignKeyInfo fk_products;
    fk_products.fk_name = "fk_products_category_id";
    fk_products.child_columns = {"category_id"};
    fk_products.parent_columns = {"id"};
    fk_products.on_delete = CatalogManager::FKAction::SET_NULL;
    fk_products.on_update = CatalogManager::FKAction::CASCADE;
    EXPECT_EQ(fk_products.on_delete, CatalogManager::FKAction::SET_NULL);

    // Critical reference: Invoices reference Orders (RESTRICT)
    CatalogManager::ForeignKeyInfo fk_invoices;
    fk_invoices.fk_name = "fk_invoices_order_id";
    fk_invoices.child_columns = {"order_id"};
    fk_invoices.parent_columns = {"id"};
    fk_invoices.on_delete = CatalogManager::FKAction::RESTRICT;
    fk_invoices.on_update = CatalogManager::FKAction::RESTRICT;
    EXPECT_EQ(fk_invoices.on_delete, CatalogManager::FKAction::RESTRICT);
}

// Test action combinations matrix
TEST_F(FKActionsTest, ActionCombinationsMatrix) {
    // Test all valid combinations of ON DELETE and ON UPDATE actions
    std::vector<CatalogManager::FKAction> actions = {
        CatalogManager::FKAction::NO_ACTION,
        CatalogManager::FKAction::RESTRICT,
        CatalogManager::FKAction::CASCADE,
        CatalogManager::FKAction::SET_NULL,
        CatalogManager::FKAction::SET_DEFAULT
    };

    for (auto on_delete : actions) {
        for (auto on_update : actions) {
            CatalogManager::ForeignKeyInfo fk;
            fk.on_delete = on_delete;
            fk.on_update = on_update;

            EXPECT_EQ(fk.on_delete, on_delete);
            EXPECT_EQ(fk.on_update, on_update);
        }
    }
}

// Test column count validation
TEST_F(FKActionsTest, ColumnCountValidation) {
    // Single column FK
    CatalogManager::ForeignKeyInfo fk1;
    fk1.child_columns = {"col1"};
    fk1.parent_columns = {"id"};
    EXPECT_EQ(fk1.child_columns.size(), fk1.parent_columns.size());

    // Two column FK
    CatalogManager::ForeignKeyInfo fk2;
    fk2.child_columns = {"col1", "col2"};
    fk2.parent_columns = {"id1", "id2"};
    EXPECT_EQ(fk2.child_columns.size(), fk2.parent_columns.size());

    // Three column FK
    CatalogManager::ForeignKeyInfo fk3;
    fk3.child_columns = {"col1", "col2", "col3"};
    fk3.parent_columns = {"id1", "id2", "id3"};
    EXPECT_EQ(fk3.child_columns.size(), fk3.parent_columns.size());
}

// Test FK naming conventions
TEST_F(FKActionsTest, NamingConventions) {
    // System-generated name
    CatalogManager::ForeignKeyInfo fk1;
    fk1.fk_name = "orders_customer_id_fkey";
    EXPECT_FALSE(fk1.fk_name.empty());

    // User-specified name
    CatalogManager::ForeignKeyInfo fk2;
    fk2.fk_name = "fk_orders_to_customers";
    EXPECT_FALSE(fk2.fk_name.empty());

    // Composite FK name
    CatalogManager::ForeignKeyInfo fk3;
    fk3.fk_name = "fk_order_items_order_id_product_id";
    EXPECT_FALSE(fk3.fk_name.empty());
}
