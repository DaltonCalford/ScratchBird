-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Constraints - FOREIGN KEY
-- Description: Comprehensive FOREIGN KEY constraint testing
-- ============================================================================

-- FOREIGN KEY: Ensures referential integrity between tables
-- Supports CASCADE, SET NULL, SET DEFAULT, RESTRICT, NO ACTION
-- Can be single-column or multi-column
-- Referenced column(s) must have UNIQUE or PRIMARY KEY constraint

-- Create test database
CREATE DATABASE test_foreign_key_constraints_db;
USE test_foreign_key_constraints_db;

-- ============================================================================
-- Section 1: Basic FOREIGN KEY
-- ============================================================================

CREATE TABLE test_departments (
    dept_id SERIAL PRIMARY KEY,
    dept_name VARCHAR(100) NOT NULL
);

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200) NOT NULL,
    dept_id INT,
    FOREIGN KEY (dept_id) REFERENCES test_departments(dept_id)
);

INSERT INTO test_departments (dept_name) VALUES ('Engineering'), ('Sales'), ('HR');
INSERT INTO test_employees (emp_name, dept_id) VALUES
    ('Alice', 1),
    ('Bob', 1),
    ('Charlie', 2);

SELECT e.emp_id, e.emp_name, d.dept_name
FROM test_employees e
JOIN test_departments d ON e.dept_id = d.dept_id
ORDER BY e.emp_id;

-- Invalid: Reference non-existent department (would fail)
-- INSERT INTO test_employees (emp_name, dept_id) VALUES ('Diana', 999);

-- Valid: NULL foreign key (orphan record)
INSERT INTO test_employees (emp_name, dept_id) VALUES ('Eve', NULL);

-- ============================================================================
-- Section 2: Named FOREIGN KEY Constraint
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200) NOT NULL
);

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    order_date DATE NOT NULL,
    customer_id INT,
    CONSTRAINT fk_order_customer FOREIGN KEY (customer_id) REFERENCES test_customers(customer_id)
);

INSERT INTO test_customers (customer_name) VALUES ('Customer A'), ('Customer B');
INSERT INTO test_orders (order_date, customer_id) VALUES
    ('2024-01-15', 1),
    ('2024-01-20', 1),
    ('2024-01-25', 2);

-- Query constraint info
SELECT conname, contype, confupdtype, confdeltype
FROM pg_constraint
WHERE conname = 'fk_order_customer';

-- ============================================================================
-- Section 3: Composite FOREIGN KEY
-- ============================================================================

CREATE TABLE test_locations (
    country VARCHAR(50),
    state VARCHAR(50),
    city VARCHAR(100),
    PRIMARY KEY (country, state, city)
);

CREATE TABLE test_stores (
    store_id SERIAL PRIMARY KEY,
    store_name VARCHAR(200),
    country VARCHAR(50),
    state VARCHAR(50),
    city VARCHAR(100),
    FOREIGN KEY (country, state, city) REFERENCES test_locations(country, state, city)
);

INSERT INTO test_locations (country, state, city) VALUES
    ('USA', 'CA', 'San Francisco'),
    ('USA', 'NY', 'New York'),
    ('USA', 'IL', 'Chicago');

INSERT INTO test_stores (store_name, country, state, city) VALUES
    ('Store SF', 'USA', 'CA', 'San Francisco'),
    ('Store NY', 'USA', 'NY', 'New York');

SELECT store_id, store_name, country, state, city FROM test_stores ORDER BY store_id;

-- Invalid: Non-existent location (would fail)
-- INSERT INTO test_stores (store_name, country, state, city) VALUES ('Store LA', 'USA', 'CA', 'Los Angeles');

-- ============================================================================
-- Section 4: ON DELETE CASCADE
-- ============================================================================

CREATE TABLE test_authors (
    author_id SERIAL PRIMARY KEY,
    author_name VARCHAR(200) NOT NULL
);

CREATE TABLE test_books (
    book_id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    author_id INT,
    FOREIGN KEY (author_id) REFERENCES test_authors(author_id) ON DELETE CASCADE
);

INSERT INTO test_authors (author_name) VALUES ('Author A'), ('Author B');
INSERT INTO test_books (title, author_id) VALUES
    ('Book 1', 1),
    ('Book 2', 1),
    ('Book 3', 2);

-- Delete author - books cascade delete
DELETE FROM test_authors WHERE author_id = 1;

-- Books by Author A are deleted
SELECT book_id, title, author_id FROM test_books ORDER BY book_id;

-- ============================================================================
-- Section 5: ON DELETE SET NULL
-- ============================================================================

CREATE TABLE test_teams (
    team_id SERIAL PRIMARY KEY,
    team_name VARCHAR(100) NOT NULL
);

CREATE TABLE test_players (
    player_id SERIAL PRIMARY KEY,
    player_name VARCHAR(200) NOT NULL,
    team_id INT,
    FOREIGN KEY (team_id) REFERENCES test_teams(team_id) ON DELETE SET NULL
);

INSERT INTO test_teams (team_name) VALUES ('Team A'), ('Team B');
INSERT INTO test_players (player_name, team_id) VALUES
    ('Player 1', 1),
    ('Player 2', 1),
    ('Player 3', 2);

-- Delete team - player team_id set to NULL
DELETE FROM test_teams WHERE team_id = 1;

SELECT player_id, player_name, team_id FROM test_players ORDER BY player_id;

-- ============================================================================
-- Section 6: ON DELETE SET DEFAULT
-- ============================================================================

CREATE TABLE test_categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(100) NOT NULL
);

INSERT INTO test_categories (category_id, category_name) VALUES (1, 'Uncategorized'), (2, 'Electronics'), (3, 'Books');

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200) NOT NULL,
    category_id INT DEFAULT 1,
    FOREIGN KEY (category_id) REFERENCES test_categories(category_id) ON DELETE SET DEFAULT
);

INSERT INTO test_products (product_name, category_id) VALUES
    ('Product A', 2),
    ('Product B', 2),
    ('Product C', 3);

-- Delete category - products set to default category (1)
DELETE FROM test_categories WHERE category_id = 2;

SELECT product_id, product_name, category_id FROM test_products ORDER BY product_id;

-- ============================================================================
-- Section 7: ON DELETE RESTRICT
-- ============================================================================

CREATE TABLE test_warehouses (
    warehouse_id SERIAL PRIMARY KEY,
    warehouse_name VARCHAR(100) NOT NULL
);

CREATE TABLE test_inventory (
    inventory_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200) NOT NULL,
    warehouse_id INT,
    FOREIGN KEY (warehouse_id) REFERENCES test_warehouses(warehouse_id) ON DELETE RESTRICT
);

INSERT INTO test_warehouses (warehouse_name) VALUES ('Warehouse A'), ('Warehouse B');
INSERT INTO test_inventory (item_name, warehouse_id) VALUES
    ('Item 1', 1),
    ('Item 2', 1);

-- Cannot delete warehouse with inventory (would fail)
-- DELETE FROM test_warehouses WHERE warehouse_id = 1;

-- Must delete inventory first
DELETE FROM test_inventory WHERE warehouse_id = 1;

-- Now can delete warehouse
DELETE FROM test_warehouses WHERE warehouse_id = 1;

SELECT warehouse_id, warehouse_name FROM test_warehouses;

-- ============================================================================
-- Section 8: ON DELETE NO ACTION (default)
-- ============================================================================

CREATE TABLE test_suppliers (
    supplier_id SERIAL PRIMARY KEY,
    supplier_name VARCHAR(100) NOT NULL
);

CREATE TABLE test_parts (
    part_id SERIAL PRIMARY KEY,
    part_name VARCHAR(200) NOT NULL,
    supplier_id INT,
    FOREIGN KEY (supplier_id) REFERENCES test_suppliers(supplier_id) ON DELETE NO ACTION
);

INSERT INTO test_suppliers (supplier_name) VALUES ('Supplier A'), ('Supplier B');
INSERT INTO test_parts (part_name, supplier_id) VALUES ('Part 1', 1), ('Part 2', 2);

-- Cannot delete supplier with parts (would fail)
-- DELETE FROM test_suppliers WHERE supplier_id = 1;

-- ============================================================================
-- Section 9: ON UPDATE CASCADE
-- ============================================================================

CREATE TABLE test_countries (
    country_code VARCHAR(3) PRIMARY KEY,
    country_name VARCHAR(100) NOT NULL
);

CREATE TABLE test_cities (
    city_id SERIAL PRIMARY KEY,
    city_name VARCHAR(100) NOT NULL,
    country_code VARCHAR(3),
    FOREIGN KEY (country_code) REFERENCES test_countries(country_code) ON UPDATE CASCADE
);

INSERT INTO test_countries (country_code, country_name) VALUES ('USA', 'United States'), ('CAN', 'Canada');
INSERT INTO test_cities (city_name, country_code) VALUES
    ('New York', 'USA'),
    ('Los Angeles', 'USA'),
    ('Toronto', 'CAN');

-- Update country code - cascades to cities
UPDATE test_countries SET country_code = 'US' WHERE country_code = 'USA';

SELECT city_name, country_code FROM test_cities ORDER BY city_id;

-- ============================================================================
-- Section 10: ON UPDATE SET NULL
-- ============================================================================

CREATE TABLE test_regions (
    region_code VARCHAR(10) PRIMARY KEY,
    region_name VARCHAR(100) NOT NULL
);

CREATE TABLE test_offices (
    office_id SERIAL PRIMARY KEY,
    office_name VARCHAR(100) NOT NULL,
    region_code VARCHAR(10),
    FOREIGN KEY (region_code) REFERENCES test_regions(region_code) ON UPDATE SET NULL
);

INSERT INTO test_regions (region_code, region_name) VALUES ('WEST', 'Western Region'), ('EAST', 'Eastern Region');
INSERT INTO test_offices (office_name, region_code) VALUES
    ('Office A', 'WEST'),
    ('Office B', 'WEST');

-- Update region code - office region_code set to NULL
UPDATE test_regions SET region_code = 'WEST2' WHERE region_code = 'WEST';

SELECT office_id, office_name, region_code FROM test_offices ORDER BY office_id;

-- ============================================================================
-- Section 11: Self-Referencing FOREIGN KEY
-- ============================================================================

CREATE TABLE test_org_hierarchy (
    employee_id SERIAL PRIMARY KEY,
    employee_name VARCHAR(200) NOT NULL,
    manager_id INT,
    FOREIGN KEY (manager_id) REFERENCES test_org_hierarchy(employee_id)
);

INSERT INTO test_org_hierarchy (employee_id, employee_name, manager_id) VALUES
    (1, 'CEO', NULL),
    (2, 'VP Engineering', 1),
    (3, 'VP Sales', 1),
    (4, 'Engineer 1', 2),
    (5, 'Engineer 2', 2),
    (6, 'Sales Rep 1', 3);

SELECT
    e.employee_id,
    e.employee_name,
    m.employee_name AS manager_name
FROM test_org_hierarchy e
LEFT JOIN test_org_hierarchy m ON e.manager_id = m.employee_id
ORDER BY e.employee_id;

-- ============================================================================
-- Section 12: Multiple FOREIGN KEYs to Same Table
-- ============================================================================

CREATE TABLE test_users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100) NOT NULL
);

CREATE TABLE test_messages (
    message_id SERIAL PRIMARY KEY,
    sender_id INT,
    receiver_id INT,
    message_text TEXT,
    FOREIGN KEY (sender_id) REFERENCES test_users(user_id),
    FOREIGN KEY (receiver_id) REFERENCES test_users(user_id)
);

INSERT INTO test_users (username) VALUES ('Alice'), ('Bob'), ('Charlie');
INSERT INTO test_messages (sender_id, receiver_id, message_text) VALUES
    (1, 2, 'Hello Bob from Alice'),
    (2, 1, 'Hi Alice from Bob'),
    (1, 3, 'Hi Charlie from Alice');

SELECT
    m.message_id,
    s.username AS sender,
    r.username AS receiver,
    m.message_text
FROM test_messages m
JOIN test_users s ON m.sender_id = s.user_id
JOIN test_users r ON m.receiver_id = r.user_id
ORDER BY m.message_id;

-- ============================================================================
-- Section 13: FOREIGN KEY to UNIQUE (not PRIMARY KEY)
-- ============================================================================

CREATE TABLE test_persons (
    person_id SERIAL PRIMARY KEY,
    email VARCHAR(200) UNIQUE NOT NULL,
    person_name VARCHAR(200)
);

CREATE TABLE test_contact_log (
    log_id SERIAL PRIMARY KEY,
    contact_email VARCHAR(200),
    contact_date DATE,
    notes TEXT,
    FOREIGN KEY (contact_email) REFERENCES test_persons(email)
);

INSERT INTO test_persons (email, person_name) VALUES
    ('alice@example.com', 'Alice'),
    ('bob@example.com', 'Bob');

INSERT INTO test_contact_log (contact_email, contact_date, notes) VALUES
    ('alice@example.com', '2024-01-15', 'Called Alice'),
    ('bob@example.com', '2024-01-16', 'Emailed Bob');

SELECT cl.log_id, p.person_name, cl.contact_date, cl.notes
FROM test_contact_log cl
JOIN test_persons p ON cl.contact_email = p.email
ORDER BY cl.log_id;

-- ============================================================================
-- Section 14: Adding FOREIGN KEY to Existing Table
-- ============================================================================

CREATE TABLE test_projects (
    project_id SERIAL PRIMARY KEY,
    project_name VARCHAR(200) NOT NULL
);

CREATE TABLE test_tasks (
    task_id SERIAL PRIMARY KEY,
    task_name VARCHAR(200) NOT NULL,
    project_id INT
);

INSERT INTO test_projects (project_name) VALUES ('Project Alpha'), ('Project Beta');
INSERT INTO test_tasks (task_name, project_id) VALUES
    ('Task 1', 1),
    ('Task 2', 1),
    ('Task 3', 2);

-- Add foreign key constraint
ALTER TABLE test_tasks ADD CONSTRAINT fk_task_project
    FOREIGN KEY (project_id) REFERENCES test_projects(project_id);

-- Verify constraint
SELECT conname FROM pg_constraint WHERE conrelid = 'test_tasks'::regclass AND contype = 'f';

-- ============================================================================
-- Section 15: Dropping FOREIGN KEY Constraint
-- ============================================================================

CREATE TABLE test_drop_fk_parent (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_drop_fk_child (
    id SERIAL PRIMARY KEY,
    parent_id INT,
    CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES test_drop_fk_parent(id)
);

-- Drop the foreign key
ALTER TABLE test_drop_fk_child DROP CONSTRAINT fk_child_parent;

-- Now can insert orphan records
INSERT INTO test_drop_fk_parent (name) VALUES ('Parent 1');
INSERT INTO test_drop_fk_child (parent_id) VALUES (999);  -- Orphan, no constraint

SELECT id, parent_id FROM test_drop_fk_child;

-- ============================================================================
-- Section 16: Deferred FOREIGN KEY Constraints
-- ============================================================================

CREATE TABLE test_deferred_parent (
    id INT PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE test_deferred_child (
    id INT PRIMARY KEY,
    parent_id INT,
    CONSTRAINT fk_deferred FOREIGN KEY (parent_id)
        REFERENCES test_deferred_parent(id)
        DEFERRABLE INITIALLY DEFERRED
);

BEGIN;
-- Can insert child before parent (constraint checked at COMMIT)
INSERT INTO test_deferred_child (id, parent_id) VALUES (1, 100);
INSERT INTO test_deferred_parent (id, name) VALUES (100, 'Parent');
COMMIT;

SELECT c.id, c.parent_id, p.name
FROM test_deferred_child c
JOIN test_deferred_parent p ON c.parent_id = p.id;

-- ============================================================================
-- Section 17: FOREIGN KEY Validation Performance
-- ============================================================================

CREATE TABLE test_fk_performance_parent (
    id INT PRIMARY KEY
);

CREATE TABLE test_fk_performance_child (
    id SERIAL PRIMARY KEY,
    parent_id INT,
    FOREIGN KEY (parent_id) REFERENCES test_fk_performance_parent(id)
);

-- Insert parent records
INSERT INTO test_fk_performance_parent SELECT i FROM generate_series(1, 1000) i;

-- Each child insert validates FK (uses parent table index)
INSERT INTO test_fk_performance_child (parent_id)
SELECT (RANDOM() * 999 + 1)::INT FROM generate_series(1, 5000);

SELECT COUNT(*) FROM test_fk_performance_child;

-- ============================================================================
-- Section 18: Circular FOREIGN KEY Dependencies
-- ============================================================================

CREATE TABLE test_circular_a (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    b_id INT
);

CREATE TABLE test_circular_b (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    a_id INT
);

-- Add FKs after both tables exist
ALTER TABLE test_circular_a ADD CONSTRAINT fk_a_to_b
    FOREIGN KEY (b_id) REFERENCES test_circular_b(id) DEFERRABLE INITIALLY DEFERRED;

ALTER TABLE test_circular_b ADD CONSTRAINT fk_b_to_a
    FOREIGN KEY (a_id) REFERENCES test_circular_a(id) DEFERRABLE INITIALLY DEFERRED;

-- Use deferred constraints for circular inserts
BEGIN;
SET CONSTRAINTS ALL DEFERRED;
INSERT INTO test_circular_a (id, name, b_id) VALUES (1, 'A1', 1);
INSERT INTO test_circular_b (id, name, a_id) VALUES (1, 'B1', 1);
COMMIT;

SELECT * FROM test_circular_a;
SELECT * FROM test_circular_b;

-- ============================================================================
-- Section 19: FOREIGN KEY with Different Actions
-- ============================================================================

CREATE TABLE test_action_combinations_parent (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    name VARCHAR(100)
);

CREATE TABLE test_action_combinations_child (
    id SERIAL PRIMARY KEY,
    parent_id INT,
    parent_code VARCHAR(50),
    data TEXT,
    -- Different actions for different FKs
    CONSTRAINT fk1 FOREIGN KEY (parent_id)
        REFERENCES test_action_combinations_parent(id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk2 FOREIGN KEY (parent_code)
        REFERENCES test_action_combinations_parent(code)
        ON DELETE SET NULL ON UPDATE SET NULL
);

INSERT INTO test_action_combinations_parent (code, name) VALUES ('CODE1', 'Parent 1');
INSERT INTO test_action_combinations_child (parent_id, parent_code, data)
VALUES (1, 'CODE1', 'Data 1');

-- Update parent - different actions for each FK
UPDATE test_action_combinations_parent SET code = 'CODE2' WHERE id = 1;

SELECT id, parent_id, parent_code, data FROM test_action_combinations_child;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_fk_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_fk_best_practices VALUES
    (1, 'Use FOREIGN KEYs to enforce referential integrity'),
    (2, 'Choose appropriate ON DELETE action for business logic'),
    (3, 'CASCADE: Child records deleted with parent'),
    (4, 'SET NULL: Child FKs set to NULL when parent deleted'),
    (5, 'SET DEFAULT: Child FKs set to default when parent deleted'),
    (6, 'RESTRICT/NO ACTION: Prevent parent deletion if children exist'),
    (7, 'Index foreign key columns for better performance'),
    (8, 'Use composite FKs when natural key has multiple columns'),
    (9, 'Name FK constraints explicitly for clarity'),
    (10, 'Use DEFERRABLE for circular dependencies'),
    (11, 'FK can reference UNIQUE constraint, not just PRIMARY KEY'),
    (12, 'NULL values in FK columns create orphan records'),
    (13, 'Test cascade behavior thoroughly'),
    (14, 'Document FK relationships in ER diagrams'),
    (15, 'Consider performance impact of FK validation on inserts');

SELECT id, guideline FROM test_fk_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_fk_best_practices;
DROP TABLE test_action_combinations_child;
DROP TABLE test_action_combinations_parent;
DROP TABLE test_circular_b;
DROP TABLE test_circular_a;
DROP TABLE test_fk_performance_child;
DROP TABLE test_fk_performance_parent;
DROP TABLE test_deferred_child;
DROP TABLE test_deferred_parent;
DROP TABLE test_drop_fk_child;
DROP TABLE test_drop_fk_parent;
DROP TABLE test_tasks;
DROP TABLE test_projects;
DROP TABLE test_contact_log;
DROP TABLE test_persons;
DROP TABLE test_messages;
DROP TABLE test_users;
DROP TABLE test_org_hierarchy;
DROP TABLE test_offices;
DROP TABLE test_regions;
DROP TABLE test_cities;
DROP TABLE test_countries;
DROP TABLE test_parts;
DROP TABLE test_suppliers;
DROP TABLE test_inventory;
DROP TABLE test_warehouses;
DROP TABLE test_products;
DROP TABLE test_categories;
DROP TABLE test_players;
DROP TABLE test_teams;
DROP TABLE test_books;
DROP TABLE test_authors;
DROP TABLE test_stores;
DROP TABLE test_locations;
DROP TABLE test_orders;
DROP TABLE test_customers;
DROP TABLE test_employees;
DROP TABLE test_departments;

DROP DATABASE test_foreign_key_constraints_db;

-- End of FOREIGN KEY constraint tests
