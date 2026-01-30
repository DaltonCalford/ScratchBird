-- Test Category: Triggers - INSTEAD OF (View Update/Delete Row + Statement)
-- Description: Targeted tests for INSTEAD OF UPDATE/DELETE on views (row + statement).

-- Create test database
CREATE DATABASE test_instead_of_view_update_delete_db;
USE test_instead_of_view_update_delete_db;

-- Section 1: Row-level INSTEAD OF UPDATE (per-row OLD/NEW)
CREATE TABLE test_view_ud_base (
    id INT PRIMARY KEY,
    val INT,
    updated_at TIMESTAMP
);

INSERT INTO test_view_ud_base (id, val) VALUES
    (1, 10),
    (2, 20),
    (3, 30);

CREATE VIEW test_view_ud_view AS
SELECT id, val
FROM test_view_ud_base;

CREATE TABLE test_view_ud_log (
    event VARCHAR(32),
    id INT,
    old_val INT,
    new_val INT
);

CREATE FUNCTION test_ud_row_update() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_view_ud_log (event, id, old_val, new_val)
    VALUES ('ROW_UPDATE', OLD.id, OLD.val, NEW.val);

    UPDATE test_view_ud_base
    SET val = NEW.val, updated_at = CURRENT_TIMESTAMP
    WHERE id = OLD.id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_ud_row_update
INSTEAD OF UPDATE ON test_view_ud_view
FOR EACH ROW
EXECUTE FUNCTION test_ud_row_update();

UPDATE test_view_ud_view SET val = val + 100 WHERE id IN (1, 3);

SELECT id, val FROM test_view_ud_base ORDER BY id;
SELECT event, id, old_val, new_val FROM test_view_ud_log ORDER BY id;

-- Section 2: Statement-level INSTEAD OF UPDATE
CREATE TABLE test_view_ud_stmt_base (
    id INT PRIMARY KEY,
    val INT
);

INSERT INTO test_view_ud_stmt_base (id, val) VALUES
    (1, 5),
    (2, 6);

CREATE VIEW test_view_ud_stmt_view AS
SELECT id, val FROM test_view_ud_stmt_base;

CREATE TABLE test_view_ud_stmt_log (
    marker VARCHAR(32),
    old_rows INT,
    new_rows INT
);

CREATE FUNCTION test_ud_stmt_update() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_view_ud_stmt_log (marker, old_rows, new_rows)
    VALUES (
        'STMT_UPDATE',
        (SELECT COUNT(*) FROM test_view_ud_stmt_view),
        (SELECT COUNT(*) FROM test_view_ud_stmt_view)
    );
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_ud_stmt_update
INSTEAD OF UPDATE ON test_view_ud_stmt_view
FOR EACH STATEMENT
EXECUTE FUNCTION test_ud_stmt_update();

UPDATE test_view_ud_stmt_view SET val = val + 1;
SELECT marker, old_rows, new_rows FROM test_view_ud_stmt_log ORDER BY marker;

-- Section 3: Row-level INSTEAD OF DELETE
CREATE TABLE test_view_del_base (
    id INT PRIMARY KEY,
    val INT
);

INSERT INTO test_view_del_base (id, val) VALUES
    (1, 11),
    (2, 22),
    (3, 33);

CREATE VIEW test_view_del_view AS
SELECT id, val FROM test_view_del_base;

CREATE TABLE test_view_del_log (
    event VARCHAR(32),
    id INT,
    old_val INT
);

CREATE FUNCTION test_ud_row_delete() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_view_del_log (event, id, old_val)
    VALUES ('ROW_DELETE', OLD.id, OLD.val);
    DELETE FROM test_view_del_base WHERE id = OLD.id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_ud_row_delete
INSTEAD OF DELETE ON test_view_del_view
FOR EACH ROW
EXECUTE FUNCTION test_ud_row_delete();

DELETE FROM test_view_del_view WHERE id IN (1, 2);

SELECT id, val FROM test_view_del_base ORDER BY id;
SELECT event, id, old_val FROM test_view_del_log ORDER BY id;

-- Section 4: Statement-level INSTEAD OF DELETE
CREATE TABLE test_view_del_stmt_base (
    id INT PRIMARY KEY,
    val INT
);

INSERT INTO test_view_del_stmt_base (id, val) VALUES
    (1, 100),
    (2, 200);

CREATE VIEW test_view_del_stmt_view AS
SELECT id, val FROM test_view_del_stmt_base;

CREATE TABLE test_view_del_stmt_log (
    marker VARCHAR(32),
    old_rows INT
);

CREATE FUNCTION test_ud_stmt_delete() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_view_del_stmt_log (marker, old_rows)
    VALUES ('STMT_DELETE', (SELECT COUNT(*) FROM test_view_del_stmt_view));
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_ud_stmt_delete
INSTEAD OF DELETE ON test_view_del_stmt_view
FOR EACH STATEMENT
EXECUTE FUNCTION test_ud_stmt_delete();

DELETE FROM test_view_del_stmt_view;
SELECT marker, old_rows FROM test_view_del_stmt_log ORDER BY marker;
