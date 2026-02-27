CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_fk_child;
DROP TABLE IF EXISTS v3inet.v3_fk_parent;

CREATE TABLE v3inet.v3_fk_parent (
    id INTEGER PRIMARY KEY,
    name VARCHAR(32)
);

CREATE TABLE v3inet.v3_fk_child (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER,
    payload VARCHAR(32),
    CONSTRAINT fk_v3_parent FOREIGN KEY(parent_id) REFERENCES v3inet.v3_fk_parent(id)
);

INSERT INTO v3inet.v3_fk_parent (id, name) VALUES
    (1, 'p1'),
    (2, 'p2'),
    (3, 'p3');

INSERT INTO v3inet.v3_fk_child (id, parent_id, payload) VALUES
    (10, 1, 'c1'),
    (11, 1, 'c2'),
    (12, 2, 'c3');

SELECT 'ASSERT|fk|parent_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_fk_parent;

SELECT 'ASSERT|fk|child_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_fk_child;

SELECT 'ASSERT|fk|child_for_parent_1|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_fk_child
WHERE parent_id = 1;

SELECT 'ASSERT|fk|join_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_fk_parent p
JOIN v3inet.v3_fk_child c ON c.parent_id = p.id;

SELECT 'ASSERT|fk|orphan_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_fk_child c
LEFT JOIN v3inet.v3_fk_parent p ON p.id = c.parent_id
WHERE p.id IS NULL;
