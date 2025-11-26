/**
 * test_constraint_enforcement.cpp - P2-14: Constraint Enforcement Tests
 *
 * Tests for constraint validation including:
 * - CHECK constraint violations
 * - Foreign key constraint violations
 * - UNIQUE constraint violations
 * - NOT NULL constraint violations
 * - IDENTITY column behavior
 * - Deferred constraint checking
 *
 * P2-14: Constraint Enforcement Tests
 * Created: November 25, 2025
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <set>
#include <functional>
#include <cstdint>

// =============================================================================
// CONSTRAINT TYPES
// =============================================================================

enum class ConstraintType
{
    NOT_NULL,
    CHECK,
    UNIQUE,
    PRIMARY_KEY,
    FOREIGN_KEY
};

enum class DeferMode
{
    NOT_DEFERRABLE,
    DEFERRABLE_INITIALLY_IMMEDIATE,
    DEFERRABLE_INITIALLY_DEFERRED
};

// =============================================================================
// CONSTRAINT DEFINITIONS
// =============================================================================

struct CheckConstraint
{
    std::string name;
    std::function<bool(int64_t)> predicate;  // Simplified for testing
    DeferMode defer_mode;
};

struct UniqueConstraint
{
    std::string name;
    std::vector<std::string> columns;
    DeferMode defer_mode;
};

struct ForeignKeyConstraint
{
    std::string name;
    std::string local_column;
    std::string ref_table;
    std::string ref_column;
    DeferMode defer_mode;
};

struct NotNullConstraint
{
    std::string column;
};

// =============================================================================
// SIMPLE TABLE MODEL FOR TESTING
// =============================================================================

class SimpleTable
{
public:
    struct Row
    {
        int64_t id;
        std::optional<int64_t> value;
        std::optional<std::string> name;
        std::optional<int64_t> fk_id;
    };

    void addCheckConstraint(const CheckConstraint& c) { check_constraints_.push_back(c); }
    void addUniqueConstraint(const UniqueConstraint& c) { unique_constraints_.push_back(c); }
    void addForeignKeyConstraint(const ForeignKeyConstraint& c) { fk_constraints_.push_back(c); }
    void addNotNullConstraint(const NotNullConstraint& c) { not_null_columns_.insert(c.column); }

    void setForeignTable(SimpleTable* table) { foreign_table_ = table; }

    std::pair<bool, std::string> insertRow(const Row& row, bool defer_all = false)
    {
        // Check NOT NULL constraints
        for (const auto& col : not_null_columns_)
        {
            if (col == "value" && !row.value.has_value())
            {
                return {false, "NOT NULL violation: column 'value' cannot be null"};
            }
            if (col == "name" && !row.name.has_value())
            {
                return {false, "NOT NULL violation: column 'name' cannot be null"};
            }
            if (col == "fk_id" && !row.fk_id.has_value())
            {
                return {false, "NOT NULL violation: column 'fk_id' cannot be null"};
            }
        }

        // Check CHECK constraints
        for (const auto& c : check_constraints_)
        {
            bool should_defer = defer_all ||
                               c.defer_mode == DeferMode::DEFERRABLE_INITIALLY_DEFERRED;
            if (!should_defer)
            {
                if (row.value.has_value() && !c.predicate(*row.value))
                {
                    return {false, "CHECK constraint '" + c.name + "' violation"};
                }
            }
            else
            {
                // Store for deferred checking
                deferred_checks_.push_back([c, row]() -> std::pair<bool, std::string> {
                    if (row.value.has_value() && !c.predicate(*row.value))
                    {
                        return {false, "CHECK constraint '" + c.name + "' violation (deferred)"};
                    }
                    return {true, ""};
                });
            }
        }

        // Check UNIQUE constraints
        for (const auto& c : unique_constraints_)
        {
            bool should_defer = defer_all ||
                               c.defer_mode == DeferMode::DEFERRABLE_INITIALLY_DEFERRED;
            if (!should_defer)
            {
                if (c.columns[0] == "id")
                {
                    for (const auto& existing : rows_)
                    {
                        if (existing.id == row.id)
                        {
                            return {false, "UNIQUE constraint '" + c.name + "' violation on column 'id'"};
                        }
                    }
                }
                if (c.columns[0] == "value" && row.value.has_value())
                {
                    for (const auto& existing : rows_)
                    {
                        if (existing.value.has_value() && *existing.value == *row.value)
                        {
                            return {false, "UNIQUE constraint '" + c.name + "' violation on column 'value'"};
                        }
                    }
                }
            }
        }

        // Check FK constraints
        for (const auto& c : fk_constraints_)
        {
            bool should_defer = defer_all ||
                               c.defer_mode == DeferMode::DEFERRABLE_INITIALLY_DEFERRED;
            if (!should_defer && foreign_table_ != nullptr)
            {
                if (row.fk_id.has_value())
                {
                    bool found = false;
                    for (const auto& ref_row : foreign_table_->rows_)
                    {
                        if (ref_row.id == *row.fk_id)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        return {false, "FOREIGN KEY constraint '" + c.name +
                                       "' violation: referenced row not found"};
                    }
                }
            }
        }

        rows_.push_back(row);
        return {true, ""};
    }

    std::pair<bool, std::string> validateDeferredConstraints()
    {
        for (const auto& check : deferred_checks_)
        {
            auto [ok, msg] = check();
            if (!ok)
            {
                return {false, msg};
            }
        }
        deferred_checks_.clear();
        return {true, ""};
    }

    size_t rowCount() const { return rows_.size(); }
    const std::vector<Row>& rows() const { return rows_; }
    void clear()
    {
        rows_.clear();
        deferred_checks_.clear();
    }

private:
    std::vector<Row> rows_;
    std::vector<CheckConstraint> check_constraints_;
    std::vector<UniqueConstraint> unique_constraints_;
    std::vector<ForeignKeyConstraint> fk_constraints_;
    std::set<std::string> not_null_columns_;
    SimpleTable* foreign_table_ = nullptr;
    std::vector<std::function<std::pair<bool, std::string>()>> deferred_checks_;
};

// =============================================================================
// CHECK CONSTRAINT TESTS
// =============================================================================

class CheckConstraintTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        table_.clear();
    }

    SimpleTable table_;
};

TEST_F(CheckConstraintTest, PositiveValue_Passes)
{
    CheckConstraint c{"positive_check", [](int64_t v) { return v > 0; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c);

    auto [ok, msg] = table_.insertRow({1, 100, std::nullopt, std::nullopt});
    EXPECT_TRUE(ok) << msg;
}

TEST_F(CheckConstraintTest, PositiveValue_Fails)
{
    CheckConstraint c{"positive_check", [](int64_t v) { return v > 0; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c);

    auto [ok, msg] = table_.insertRow({1, -5, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok);
    EXPECT_TRUE(msg.find("CHECK constraint") != std::string::npos);
}

TEST_F(CheckConstraintTest, RangeCheck_Passes)
{
    CheckConstraint c{"range_check", [](int64_t v) { return v >= 0 && v <= 100; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c);

    auto [ok1, _1] = table_.insertRow({1, 0, std::nullopt, std::nullopt});
    auto [ok2, _2] = table_.insertRow({2, 50, std::nullopt, std::nullopt});
    auto [ok3, _3] = table_.insertRow({3, 100, std::nullopt, std::nullopt});

    EXPECT_TRUE(ok1);
    EXPECT_TRUE(ok2);
    EXPECT_TRUE(ok3);
}

TEST_F(CheckConstraintTest, RangeCheck_Fails_Above)
{
    CheckConstraint c{"range_check", [](int64_t v) { return v >= 0 && v <= 100; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c);

    auto [ok, msg] = table_.insertRow({1, 101, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok);
}

TEST_F(CheckConstraintTest, RangeCheck_Fails_Below)
{
    CheckConstraint c{"range_check", [](int64_t v) { return v >= 0 && v <= 100; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c);

    auto [ok, msg] = table_.insertRow({1, -1, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok);
}

TEST_F(CheckConstraintTest, MultipleConstraints_AllMustPass)
{
    CheckConstraint c1{"positive", [](int64_t v) { return v > 0; }, DeferMode::NOT_DEFERRABLE};
    CheckConstraint c2{"even", [](int64_t v) { return v % 2 == 0; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c1);
    table_.addCheckConstraint(c2);

    auto [ok1, _1] = table_.insertRow({1, 2, std::nullopt, std::nullopt});  // Positive and even
    EXPECT_TRUE(ok1);

    auto [ok2, _2] = table_.insertRow({2, 3, std::nullopt, std::nullopt});  // Positive but odd
    EXPECT_FALSE(ok2);

    auto [ok3, _3] = table_.insertRow({3, -2, std::nullopt, std::nullopt}); // Negative
    EXPECT_FALSE(ok3);
}

// =============================================================================
// NOT NULL CONSTRAINT TESTS
// =============================================================================

class NotNullConstraintTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        table_.clear();
    }

    SimpleTable table_;
};

TEST_F(NotNullConstraintTest, NonNullValue_Passes)
{
    table_.addNotNullConstraint({"value"});

    auto [ok, msg] = table_.insertRow({1, 42, std::nullopt, std::nullopt});
    EXPECT_TRUE(ok);
}

TEST_F(NotNullConstraintTest, NullValue_Fails)
{
    table_.addNotNullConstraint({"value"});

    auto [ok, msg] = table_.insertRow({1, std::nullopt, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok);
    EXPECT_TRUE(msg.find("NOT NULL") != std::string::npos);
}

TEST_F(NotNullConstraintTest, MultipleNotNull_BothRequired)
{
    table_.addNotNullConstraint({"value"});
    table_.addNotNullConstraint({"name"});

    auto [ok1, _1] = table_.insertRow({1, 42, "test", std::nullopt});
    EXPECT_TRUE(ok1);

    auto [ok2, _2] = table_.insertRow({2, std::nullopt, "test", std::nullopt});
    EXPECT_FALSE(ok2);

    auto [ok3, _3] = table_.insertRow({3, 42, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok3);
}

// =============================================================================
// UNIQUE CONSTRAINT TESTS
// =============================================================================

class UniqueConstraintTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        table_.clear();
    }

    SimpleTable table_;
};

TEST_F(UniqueConstraintTest, UniqueValues_Pass)
{
    UniqueConstraint c{"unique_id", {"id"}, DeferMode::NOT_DEFERRABLE};
    table_.addUniqueConstraint(c);

    auto [ok1, _1] = table_.insertRow({1, 100, std::nullopt, std::nullopt});
    auto [ok2, _2] = table_.insertRow({2, 200, std::nullopt, std::nullopt});
    auto [ok3, _3] = table_.insertRow({3, 300, std::nullopt, std::nullopt});

    EXPECT_TRUE(ok1);
    EXPECT_TRUE(ok2);
    EXPECT_TRUE(ok3);
}

TEST_F(UniqueConstraintTest, DuplicateKey_Fails)
{
    UniqueConstraint c{"unique_id", {"id"}, DeferMode::NOT_DEFERRABLE};
    table_.addUniqueConstraint(c);

    auto [ok1, _1] = table_.insertRow({1, 100, std::nullopt, std::nullopt});
    EXPECT_TRUE(ok1);

    auto [ok2, msg] = table_.insertRow({1, 200, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok2);
    EXPECT_TRUE(msg.find("UNIQUE constraint") != std::string::npos);
}

TEST_F(UniqueConstraintTest, UniqueOnNonKeyColumn)
{
    UniqueConstraint c{"unique_value", {"value"}, DeferMode::NOT_DEFERRABLE};
    table_.addUniqueConstraint(c);

    auto [ok1, _1] = table_.insertRow({1, 100, std::nullopt, std::nullopt});
    EXPECT_TRUE(ok1);

    auto [ok2, _2] = table_.insertRow({2, 200, std::nullopt, std::nullopt});
    EXPECT_TRUE(ok2);

    auto [ok3, msg] = table_.insertRow({3, 100, std::nullopt, std::nullopt});  // Duplicate value
    EXPECT_FALSE(ok3);
}

TEST_F(UniqueConstraintTest, NullsAreUnique)
{
    UniqueConstraint c{"unique_value", {"value"}, DeferMode::NOT_DEFERRABLE};
    table_.addUniqueConstraint(c);

    // Multiple NULLs should be allowed (SQL standard)
    auto [ok1, _1] = table_.insertRow({1, std::nullopt, std::nullopt, std::nullopt});
    auto [ok2, _2] = table_.insertRow({2, std::nullopt, std::nullopt, std::nullopt});

    EXPECT_TRUE(ok1);
    EXPECT_TRUE(ok2);
}

// =============================================================================
// FOREIGN KEY CONSTRAINT TESTS
// =============================================================================

class ForeignKeyConstraintTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        parent_table_.clear();
        child_table_.clear();
        child_table_.setForeignTable(&parent_table_);
    }

    SimpleTable parent_table_;
    SimpleTable child_table_;
};

TEST_F(ForeignKeyConstraintTest, ValidReference_Passes)
{
    // Insert parent row first
    parent_table_.insertRow({1, 100, "parent1", std::nullopt});

    ForeignKeyConstraint c{"fk_parent", "fk_id", "parent", "id", DeferMode::NOT_DEFERRABLE};
    child_table_.addForeignKeyConstraint(c);

    auto [ok, msg] = child_table_.insertRow({1, 50, "child1", 1});
    EXPECT_TRUE(ok) << msg;
}

TEST_F(ForeignKeyConstraintTest, InvalidReference_Fails)
{
    // Parent row with id=1 exists
    parent_table_.insertRow({1, 100, "parent1", std::nullopt});

    ForeignKeyConstraint c{"fk_parent", "fk_id", "parent", "id", DeferMode::NOT_DEFERRABLE};
    child_table_.addForeignKeyConstraint(c);

    // Try to reference non-existent parent id=99
    auto [ok, msg] = child_table_.insertRow({1, 50, "child1", 99});
    EXPECT_FALSE(ok);
    EXPECT_TRUE(msg.find("FOREIGN KEY") != std::string::npos);
}

TEST_F(ForeignKeyConstraintTest, NullForeignKey_Allowed)
{
    parent_table_.insertRow({1, 100, "parent1", std::nullopt});

    ForeignKeyConstraint c{"fk_parent", "fk_id", "parent", "id", DeferMode::NOT_DEFERRABLE};
    child_table_.addForeignKeyConstraint(c);

    // NULL foreign key should be allowed
    auto [ok, msg] = child_table_.insertRow({1, 50, "child1", std::nullopt});
    EXPECT_TRUE(ok);
}

// =============================================================================
// DEFERRED CONSTRAINT TESTS
// =============================================================================

class DeferredConstraintTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        table_.clear();
    }

    SimpleTable table_;
};

TEST_F(DeferredConstraintTest, ImmediateConstraint_CheckedAtInsert)
{
    CheckConstraint c{"positive", [](int64_t v) { return v > 0; }, DeferMode::NOT_DEFERRABLE};
    table_.addCheckConstraint(c);

    auto [ok, msg] = table_.insertRow({1, -5, std::nullopt, std::nullopt});
    EXPECT_FALSE(ok) << "Immediate constraint should be checked at insert time";
}

TEST_F(DeferredConstraintTest, DeferredConstraint_CheckedAtValidate)
{
    CheckConstraint c{"positive", [](int64_t v) { return v > 0; },
                     DeferMode::DEFERRABLE_INITIALLY_DEFERRED};
    table_.addCheckConstraint(c);

    // Insert should succeed (constraint is deferred)
    auto [ok, msg] = table_.insertRow({1, -5, std::nullopt, std::nullopt});
    EXPECT_TRUE(ok) << "Deferred constraint should allow insert: " << msg;

    // Validation should fail
    auto [valid, valid_msg] = table_.validateDeferredConstraints();
    EXPECT_FALSE(valid);
    EXPECT_TRUE(valid_msg.find("deferred") != std::string::npos);
}

TEST_F(DeferredConstraintTest, SetConstraintsAllDeferred)
{
    CheckConstraint c{"positive", [](int64_t v) { return v > 0; },
                     DeferMode::DEFERRABLE_INITIALLY_IMMEDIATE};
    table_.addCheckConstraint(c);

    // Without defer_all, constraint is immediate
    auto [ok1, msg1] = table_.insertRow({1, -5, std::nullopt, std::nullopt}, false);
    EXPECT_FALSE(ok1) << "Initially immediate constraint should fail at insert";

    table_.clear();

    // With defer_all=true, constraint becomes deferred
    auto [ok2, msg2] = table_.insertRow({2, -5, std::nullopt, std::nullopt}, true);
    EXPECT_TRUE(ok2) << "SET CONSTRAINTS ALL DEFERRED should defer the check";

    // Validation should fail
    auto [valid, valid_msg] = table_.validateDeferredConstraints();
    EXPECT_FALSE(valid);
}

// =============================================================================
// IDENTITY COLUMN TESTS
// =============================================================================

class IdentityColumnTest : public ::testing::Test
{
protected:
    std::atomic<int64_t> identity_counter_{0};

    int64_t getNextIdentity() { return ++identity_counter_; }
};

TEST_F(IdentityColumnTest, GeneratedAlways_AutoIncrement)
{
    std::vector<int64_t> ids;

    for (int i = 0; i < 5; i++)
    {
        ids.push_back(getNextIdentity());
    }

    // Should be sequential
    EXPECT_EQ(1, ids[0]);
    EXPECT_EQ(2, ids[1]);
    EXPECT_EQ(3, ids[2]);
    EXPECT_EQ(4, ids[3]);
    EXPECT_EQ(5, ids[4]);
}

TEST_F(IdentityColumnTest, ConcurrentIdentity_NoDuplicates)
{
    std::set<int64_t> ids;
    std::mutex ids_mutex;

    const int num_threads = 8;
    const int ids_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < ids_per_thread; j++)
            {
                int64_t id = getNextIdentity();
                std::lock_guard<std::mutex> lock(ids_mutex);
                ids.insert(id);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // All IDs should be unique
    EXPECT_EQ(num_threads * ids_per_thread, ids.size());
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
