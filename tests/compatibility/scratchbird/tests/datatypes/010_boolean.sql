-- ScratchBird Native Test: Boolean Type
-- Test ID: datatypes_010
-- Description: Comprehensive testing of BOOLEAN type with three-valued logic

CREATE DATABASE test_boolean;
\c test_boolean;

-- ============================================================================
-- BOOLEAN Basic Testing
-- ============================================================================

CREATE TABLE test_boolean_basic (
    id INTEGER PRIMARY KEY,
    value BOOLEAN,
    description VARCHAR(100)
);

-- Basic boolean values
INSERT INTO test_boolean_basic VALUES (1, TRUE, 'True value');
INSERT INTO test_boolean_basic VALUES (2, FALSE, 'False value');
INSERT INTO test_boolean_basic VALUES (3, NULL, 'Null value (unknown)');

-- Alternative representations
INSERT INTO test_boolean_basic VALUES (10, 't', 'Short true');
INSERT INTO test_boolean_basic VALUES (11, 'f', 'Short false');
INSERT INTO test_boolean_basic VALUES (12, 'true', 'String true');
INSERT INTO test_boolean_basic VALUES (13, 'false', 'String false');
INSERT INTO test_boolean_basic VALUES (14, 'yes', 'Yes as true');
INSERT INTO test_boolean_basic VALUES (15, 'no', 'No as false');
INSERT INTO test_boolean_basic VALUES (16, '1', 'One as true');
INSERT INTO test_boolean_basic VALUES (17, '0', 'Zero as false');

SELECT id, value, description FROM test_boolean_basic ORDER BY id;

-- Count by value
SELECT value, COUNT(*) AS count FROM test_boolean_basic GROUP BY value ORDER BY value;

-- ============================================================================
-- BOOLEAN Three-Valued Logic (TRUE, FALSE, NULL)
-- ============================================================================

CREATE TABLE test_three_valued_logic (
    id INTEGER PRIMARY KEY,
    a BOOLEAN,
    b BOOLEAN,
    description VARCHAR(100)
);

-- All combinations
INSERT INTO test_three_valued_logic VALUES (1, TRUE, TRUE, 'Both true');
INSERT INTO test_three_valued_logic VALUES (2, TRUE, FALSE, 'First true, second false');
INSERT INTO test_three_valued_logic VALUES (3, TRUE, NULL, 'First true, second null');
INSERT INTO test_three_valued_logic VALUES (4, FALSE, TRUE, 'First false, second true');
INSERT INTO test_three_valued_logic VALUES (5, FALSE, FALSE, 'Both false');
INSERT INTO test_three_valued_logic VALUES (6, FALSE, NULL, 'First false, second null');
INSERT INTO test_three_valued_logic VALUES (7, NULL, TRUE, 'First null, second true');
INSERT INTO test_three_valued_logic VALUES (8, NULL, FALSE, 'First null, second false');
INSERT INTO test_three_valued_logic VALUES (9, NULL, NULL, 'Both null');

-- AND truth table
SELECT id, a, b,
       a AND b AS and_result,
       description
FROM test_three_valued_logic
ORDER BY id;

-- OR truth table
SELECT id, a, b,
       a OR b AS or_result,
       description
FROM test_three_valued_logic
ORDER BY id;

-- NOT operation
SELECT id, a,
       NOT a AS not_a,
       description
FROM test_three_valued_logic
ORDER BY id;

-- ============================================================================
-- BOOLEAN Logical Operations
-- ============================================================================

CREATE TABLE test_boolean_operations (
    id INTEGER PRIMARY KEY,
    flag1 BOOLEAN,
    flag2 BOOLEAN,
    flag3 BOOLEAN
);

INSERT INTO test_boolean_operations VALUES (1, TRUE, TRUE, TRUE);
INSERT INTO test_boolean_operations VALUES (2, TRUE, TRUE, FALSE);
INSERT INTO test_boolean_operations VALUES (3, TRUE, FALSE, FALSE);
INSERT INTO test_boolean_operations VALUES (4, FALSE, FALSE, FALSE);
INSERT INTO test_boolean_operations VALUES (5, TRUE, NULL, FALSE);
INSERT INTO test_boolean_operations VALUES (6, NULL, NULL, NULL);

-- AND with multiple operands
SELECT id, flag1, flag2, flag3,
       flag1 AND flag2 AND flag3 AS all_and
FROM test_boolean_operations
ORDER BY id;

-- OR with multiple operands
SELECT id, flag1, flag2, flag3,
       flag1 OR flag2 OR flag3 AS any_or
FROM test_boolean_operations
ORDER BY id;

-- Mixed operations (AND has higher precedence than OR)
SELECT id, flag1, flag2, flag3,
       flag1 OR flag2 AND flag3 AS mixed1,
       (flag1 OR flag2) AND flag3 AS mixed2
FROM test_boolean_operations
ORDER BY id;

-- XOR (exclusive OR) - if supported
SELECT id, flag1, flag2,
       (flag1 OR flag2) AND NOT (flag1 AND flag2) AS xor_result
FROM test_boolean_operations
WHERE flag1 IS NOT NULL AND flag2 IS NOT NULL
ORDER BY id;

-- ============================================================================
-- BOOLEAN Comparison Operations
-- ============================================================================

CREATE TABLE test_boolean_comparison (
    id INTEGER PRIMARY KEY,
    status BOOLEAN,
    description VARCHAR(50)
);

INSERT INTO test_boolean_comparison VALUES (1, TRUE, 'Active');
INSERT INTO test_boolean_comparison VALUES (2, FALSE, 'Inactive');
INSERT INTO test_boolean_comparison VALUES (3, NULL, 'Unknown');
INSERT INTO test_boolean_comparison VALUES (4, TRUE, 'Enabled');
INSERT INTO test_boolean_comparison VALUES (5, FALSE, 'Disabled');

-- Equality
SELECT id, description FROM test_boolean_comparison WHERE status = TRUE ORDER BY id;
SELECT id, description FROM test_boolean_comparison WHERE status = FALSE ORDER BY id;

-- Inequality
SELECT id, description FROM test_boolean_comparison WHERE status != TRUE ORDER BY id;
SELECT id, description FROM test_boolean_comparison WHERE status <> FALSE ORDER BY id;

-- IS TRUE / IS FALSE / IS NULL
SELECT id, description, status FROM test_boolean_comparison WHERE status IS TRUE ORDER BY id;
SELECT id, description, status FROM test_boolean_comparison WHERE status IS FALSE ORDER BY id;
SELECT id, description, status FROM test_boolean_comparison WHERE status IS NULL ORDER BY id;

-- IS NOT TRUE / IS NOT FALSE / IS NOT NULL
SELECT id, description, status FROM test_boolean_comparison WHERE status IS NOT TRUE ORDER BY id;
SELECT id, description, status FROM test_boolean_comparison WHERE status IS NOT FALSE ORDER BY id;
SELECT id, description, status FROM test_boolean_comparison WHERE status IS NOT NULL ORDER BY id;

-- ============================================================================
-- BOOLEAN in WHERE Clauses
-- ============================================================================

CREATE TABLE test_boolean_where (
    id INTEGER PRIMARY KEY,
    active BOOLEAN,
    verified BOOLEAN,
    premium BOOLEAN,
    username VARCHAR(50)
);

INSERT INTO test_boolean_where VALUES (1, TRUE, TRUE, TRUE, 'user1');
INSERT INTO test_boolean_where VALUES (2, TRUE, TRUE, FALSE, 'user2');
INSERT INTO test_boolean_where VALUES (3, TRUE, FALSE, FALSE, 'user3');
INSERT INTO test_boolean_where VALUES (4, FALSE, TRUE, FALSE, 'user4');
INSERT INTO test_boolean_where VALUES (5, FALSE, FALSE, FALSE, 'user5');
INSERT INTO test_boolean_where VALUES (6, TRUE, NULL, FALSE, 'user6');
INSERT INTO test_boolean_where VALUES (7, NULL, TRUE, NULL, 'user7');

-- Single condition
SELECT username FROM test_boolean_where WHERE active ORDER BY username;
SELECT username FROM test_boolean_where WHERE NOT active ORDER BY username;

-- Multiple conditions with AND
SELECT username FROM test_boolean_where WHERE active AND verified ORDER BY username;
SELECT username FROM test_boolean_where WHERE active AND verified AND premium ORDER BY username;

-- Multiple conditions with OR
SELECT username FROM test_boolean_where WHERE active OR verified ORDER BY username;
SELECT username FROM test_boolean_where WHERE premium OR verified ORDER BY username;

-- Complex conditions
SELECT username FROM test_boolean_where
WHERE (active AND verified) OR premium
ORDER BY username;

SELECT username FROM test_boolean_where
WHERE active AND (verified OR premium)
ORDER BY username;

-- Handling NULLs in conditions
SELECT username FROM test_boolean_where WHERE active IS TRUE ORDER BY username;
SELECT username FROM test_boolean_where WHERE verified IS NOT NULL ORDER BY username;

-- ============================================================================
-- BOOLEAN Aggregation
-- ============================================================================

CREATE TABLE test_boolean_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    is_valid BOOLEAN
);

INSERT INTO test_boolean_aggregation VALUES (1, 'Group A', TRUE);
INSERT INTO test_boolean_aggregation VALUES (2, 'Group A', TRUE);
INSERT INTO test_boolean_aggregation VALUES (3, 'Group A', FALSE);
INSERT INTO test_boolean_aggregation VALUES (4, 'Group B', TRUE);
INSERT INTO test_boolean_aggregation VALUES (5, 'Group B', FALSE);
INSERT INTO test_boolean_aggregation VALUES (6, 'Group B', FALSE);
INSERT INTO test_boolean_aggregation VALUES (7, 'Group C', NULL);
INSERT INTO test_boolean_aggregation VALUES (8, 'Group C', TRUE);

-- Count by boolean value
SELECT category, is_valid, COUNT(*) AS count
FROM test_boolean_aggregation
GROUP BY category, is_valid
ORDER BY category, is_valid;

-- BOOL_AND (all true?)
SELECT category, BOOL_AND(is_valid) AS all_true
FROM test_boolean_aggregation
GROUP BY category
ORDER BY category;

-- BOOL_OR (any true?)
SELECT category, BOOL_OR(is_valid) AS any_true
FROM test_boolean_aggregation
GROUP BY category
ORDER BY category;

-- Count TRUE, FALSE, NULL separately
SELECT category,
       COUNT(CASE WHEN is_valid = TRUE THEN 1 END) AS true_count,
       COUNT(CASE WHEN is_valid = FALSE THEN 1 END) AS false_count,
       COUNT(CASE WHEN is_valid IS NULL THEN 1 END) AS null_count
FROM test_boolean_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- BOOLEAN Conversion and Casting
-- ============================================================================

CREATE TABLE test_boolean_conversion (
    id INTEGER PRIMARY KEY,
    bool_value BOOLEAN,
    int_value INTEGER,
    string_value VARCHAR(10)
);

INSERT INTO test_boolean_conversion VALUES (1, TRUE, 1, 'yes');
INSERT INTO test_boolean_conversion VALUES (2, FALSE, 0, 'no');
INSERT INTO test_boolean_conversion VALUES (3, NULL, NULL, NULL);

-- Boolean to string
SELECT id, bool_value, bool_value::VARCHAR AS as_string FROM test_boolean_conversion ORDER BY id;

-- Boolean to integer (if supported)
SELECT id, bool_value, CAST(bool_value AS INTEGER) AS as_int FROM test_boolean_conversion ORDER BY id;

-- Integer to boolean
SELECT id, int_value, CAST(int_value AS BOOLEAN) AS as_bool FROM test_boolean_conversion ORDER BY id;

-- String to boolean
SELECT id, string_value, CAST(string_value AS BOOLEAN) AS as_bool FROM test_boolean_conversion ORDER BY id;

-- ============================================================================
-- BOOLEAN in CASE Expressions
-- ============================================================================

CREATE TABLE test_boolean_case (
    id INTEGER PRIMARY KEY,
    enabled BOOLEAN,
    approved BOOLEAN
);

INSERT INTO test_boolean_case VALUES (1, TRUE, TRUE);
INSERT INTO test_boolean_case VALUES (2, TRUE, FALSE);
INSERT INTO test_boolean_case VALUES (3, FALSE, TRUE);
INSERT INTO test_boolean_case VALUES (4, FALSE, FALSE);
INSERT INTO test_boolean_case VALUES (5, NULL, TRUE);
INSERT INTO test_boolean_case VALUES (6, TRUE, NULL);

-- CASE with boolean conditions
SELECT id, enabled, approved,
       CASE
           WHEN enabled AND approved THEN 'Active and Approved'
           WHEN enabled AND NOT approved THEN 'Active but Not Approved'
           WHEN NOT enabled AND approved THEN 'Disabled but Approved'
           WHEN NOT enabled AND NOT approved THEN 'Disabled and Not Approved'
           ELSE 'Unknown Status'
       END AS status
FROM test_boolean_case
ORDER BY id;

-- Simpler CASE
SELECT id, enabled,
       CASE enabled
           WHEN TRUE THEN 'On'
           WHEN FALSE THEN 'Off'
           ELSE 'Unknown'
       END AS state
FROM test_boolean_case
ORDER BY id;

-- ============================================================================
-- BOOLEAN with NULL Handling
-- ============================================================================

CREATE TABLE test_boolean_nulls (
    id INTEGER PRIMARY KEY,
    nullable_bool BOOLEAN,
    description VARCHAR(50)
);

INSERT INTO test_boolean_nulls VALUES (1, TRUE, 'Has true value');
INSERT INTO test_boolean_nulls VALUES (2, FALSE, 'Has false value');
INSERT INTO test_boolean_nulls VALUES (3, NULL, 'Null value');
INSERT INTO test_boolean_nulls VALUES (4, NULL, 'Another null');

-- Count nulls
SELECT COUNT(*) AS null_count FROM test_boolean_nulls WHERE nullable_bool IS NULL;
SELECT COUNT(*) AS not_null_count FROM test_boolean_nulls WHERE nullable_bool IS NOT NULL;

-- COALESCE with boolean
SELECT id, nullable_bool, COALESCE(nullable_bool, FALSE) AS with_default
FROM test_boolean_nulls
ORDER BY id;

-- NULL in AND (NULL AND TRUE = NULL, NULL AND FALSE = FALSE)
SELECT id, nullable_bool,
       nullable_bool AND TRUE AS and_true,
       nullable_bool AND FALSE AS and_false,
       nullable_bool OR TRUE AS or_true,
       nullable_bool OR FALSE AS or_false
FROM test_boolean_nulls
ORDER BY id;

-- ============================================================================
-- BOOLEAN Constraints
-- ============================================================================

CREATE TABLE test_boolean_constraints (
    id INTEGER PRIMARY KEY,
    active BOOLEAN NOT NULL DEFAULT FALSE,
    verified BOOLEAN DEFAULT NULL,
    CHECK (NOT (active = TRUE AND verified = FALSE))  -- Active items must be verified
);

INSERT INTO test_boolean_constraints (id) VALUES (1);  -- Uses defaults
INSERT INTO test_boolean_constraints (id, active, verified) VALUES (2, FALSE, FALSE);
INSERT INTO test_boolean_constraints (id, active, verified) VALUES (3, FALSE, TRUE);
INSERT INTO test_boolean_constraints (id, active, verified) VALUES (4, TRUE, TRUE);

-- This should fail the CHECK constraint
-- INSERT INTO test_boolean_constraints (id, active, verified) VALUES (5, TRUE, FALSE);

SELECT id, active, verified FROM test_boolean_constraints ORDER BY id;

-- ============================================================================
-- BOOLEAN in Sorting
-- ============================================================================

CREATE TABLE test_boolean_sorting (
    id INTEGER PRIMARY KEY,
    flag BOOLEAN,
    name VARCHAR(50)
);

INSERT INTO test_boolean_sorting VALUES (1, TRUE, 'Item A');
INSERT INTO test_boolean_sorting VALUES (2, FALSE, 'Item B');
INSERT INTO test_boolean_sorting VALUES (3, NULL, 'Item C');
INSERT INTO test_boolean_sorting VALUES (4, TRUE, 'Item D');
INSERT INTO test_boolean_sorting VALUES (5, FALSE, 'Item E');

-- Sort by boolean (typically FALSE < TRUE, NULL ordering varies)
SELECT id, flag, name FROM test_boolean_sorting ORDER BY flag ASC, name;
SELECT id, flag, name FROM test_boolean_sorting ORDER BY flag DESC, name;

-- NULLS FIRST/LAST
SELECT id, flag, name FROM test_boolean_sorting ORDER BY flag ASC NULLS FIRST, name;
SELECT id, flag, name FROM test_boolean_sorting ORDER BY flag ASC NULLS LAST, name;

-- ============================================================================
-- BOOLEAN in Indexes
-- ============================================================================

CREATE TABLE test_boolean_index (
    id INTEGER PRIMARY KEY,
    is_active BOOLEAN,
    is_deleted BOOLEAN,
    data VARCHAR(100)
);

-- Create index on boolean column
CREATE INDEX idx_active ON test_boolean_index(is_active);
CREATE INDEX idx_active_not_deleted ON test_boolean_index(is_active) WHERE is_deleted = FALSE;

INSERT INTO test_boolean_index VALUES (1, TRUE, FALSE, 'Active record 1');
INSERT INTO test_boolean_index VALUES (2, TRUE, FALSE, 'Active record 2');
INSERT INTO test_boolean_index VALUES (3, FALSE, FALSE, 'Inactive record');
INSERT INTO test_boolean_index VALUES (4, TRUE, TRUE, 'Deleted record');
INSERT INTO test_boolean_index VALUES (5, FALSE, TRUE, 'Deleted inactive');

-- Query using index
SELECT id, data FROM test_boolean_index WHERE is_active = TRUE ORDER BY id;
SELECT id, data FROM test_boolean_index WHERE is_active = TRUE AND is_deleted = FALSE ORDER BY id;

-- ============================================================================
-- BOOLEAN Use Cases
-- ============================================================================

-- Feature flags
CREATE TABLE test_feature_flags (
    id INTEGER PRIMARY KEY,
    feature_name VARCHAR(50),
    enabled BOOLEAN NOT NULL DEFAULT FALSE,
    beta BOOLEAN DEFAULT FALSE
);

INSERT INTO test_feature_flags VALUES (1, 'dark_mode', TRUE, FALSE);
INSERT INTO test_feature_flags VALUES (2, 'new_ui', TRUE, TRUE);
INSERT INTO test_feature_flags VALUES (3, 'ai_assistant', FALSE, TRUE);
INSERT INTO test_feature_flags VALUES (4, 'mobile_app', FALSE, FALSE);

SELECT feature_name FROM test_feature_flags WHERE enabled ORDER BY feature_name;
SELECT feature_name FROM test_feature_flags WHERE enabled AND NOT beta ORDER BY feature_name;

-- Permission system
CREATE TABLE test_permissions (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    can_read BOOLEAN DEFAULT TRUE,
    can_write BOOLEAN DEFAULT FALSE,
    can_delete BOOLEAN DEFAULT FALSE,
    can_admin BOOLEAN DEFAULT FALSE
);

INSERT INTO test_permissions VALUES (1, 101, TRUE, TRUE, TRUE, TRUE);   -- Admin
INSERT INTO test_permissions VALUES (2, 102, TRUE, TRUE, FALSE, FALSE); -- Editor
INSERT INTO test_permissions VALUES (3, 103, TRUE, FALSE, FALSE, FALSE);-- Reader
INSERT INTO test_permissions VALUES (4, 104, FALSE, FALSE, FALSE, FALSE);-- No access

SELECT user_id,
       CASE
           WHEN can_admin THEN 'Administrator'
           WHEN can_delete THEN 'Power User'
           WHEN can_write THEN 'Editor'
           WHEN can_read THEN 'Reader'
           ELSE 'No Access'
       END AS role
FROM test_permissions
ORDER BY user_id;

-- Status tracking
CREATE TABLE test_status_tracking (
    id INTEGER PRIMARY KEY,
    item_name VARCHAR(50),
    is_complete BOOLEAN DEFAULT FALSE,
    is_approved BOOLEAN DEFAULT NULL,
    is_published BOOLEAN DEFAULT FALSE
);

INSERT INTO test_status_tracking VALUES (1, 'Task A', TRUE, TRUE, TRUE);
INSERT INTO test_status_tracking VALUES (2, 'Task B', TRUE, TRUE, FALSE);
INSERT INTO test_status_tracking VALUES (3, 'Task C', TRUE, FALSE, FALSE);
INSERT INTO test_status_tracking VALUES (4, 'Task D', FALSE, NULL, FALSE);

SELECT item_name,
       is_complete AS done,
       is_approved AS approved,
       is_published AS live,
       CASE
           WHEN is_published THEN 'Published'
           WHEN is_approved = TRUE THEN 'Ready to Publish'
           WHEN is_complete THEN 'Pending Approval'
           ELSE 'In Progress'
       END AS workflow_state
FROM test_status_tracking
ORDER BY id;

-- ============================================================================
-- BOOLEAN Short-Circuit Evaluation
-- ============================================================================

CREATE TABLE test_boolean_shortcircuit (
    id INTEGER PRIMARY KEY,
    check_expensive BOOLEAN,
    result INTEGER
);

INSERT INTO test_boolean_shortcircuit VALUES (1, FALSE, 100);
INSERT INTO test_boolean_shortcircuit VALUES (2, TRUE, 200);

-- AND short-circuits if first condition is FALSE
-- OR short-circuits if first condition is TRUE
SELECT id, check_expensive,
       CASE WHEN FALSE AND check_expensive THEN 'Should not evaluate' ELSE 'Short-circuited' END AS and_test,
       CASE WHEN TRUE OR check_expensive THEN 'Short-circuited' ELSE 'Should not evaluate' END AS or_test
FROM test_boolean_shortcircuit
ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_boolean_shortcircuit;
DROP TABLE test_status_tracking;
DROP TABLE test_permissions;
DROP TABLE test_feature_flags;
DROP INDEX idx_active_not_deleted;
DROP INDEX idx_active;
DROP TABLE test_boolean_index;
DROP TABLE test_boolean_sorting;
DROP TABLE test_boolean_constraints;
DROP TABLE test_boolean_nulls;
DROP TABLE test_boolean_case;
DROP TABLE test_boolean_conversion;
DROP TABLE test_boolean_aggregation;
DROP TABLE test_boolean_where;
DROP TABLE test_boolean_comparison;
DROP TABLE test_boolean_operations;
DROP TABLE test_three_valued_logic;
DROP TABLE test_boolean_basic;

DROP DATABASE test_boolean;
