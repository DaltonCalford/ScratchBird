CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_insert_target;
DROP TABLE IF EXISTS v3inet.v3_insert_src;

CREATE TABLE v3inet.v3_insert_target (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(64),
    amount INTEGER,
    created_at TIMESTAMP
);

CREATE TABLE v3inet.v3_insert_src (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(64),
    amount INTEGER
);

INSERT INTO v3inet.v3_insert_src (id, payload, amount) VALUES
    (101, 'src_101', 10),
    (102, 'src_102', 20),
    (103, 'src_103', 30);

INSERT INTO v3inet.v3_insert_target (id, payload, amount, created_at)
VALUES (1, 'single', 1, CURRENT_TIMESTAMP);

INSERT INTO v3inet.v3_insert_target (id, payload, amount, created_at) VALUES
    (2, 'multi_2', 2, CURRENT_TIMESTAMP),
    (3, 'multi_3', 3, CURRENT_TIMESTAMP),
    (4, 'multi_4', 4, CURRENT_TIMESTAMP);

INSERT INTO v3inet.v3_insert_target (id, payload, amount, created_at)
SELECT id, payload, amount, CURRENT_TIMESTAMP
FROM v3inet.v3_insert_src;

SELECT 'ASSERT|insert|total_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_insert_target;

SELECT 'ASSERT|insert|single_row|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_insert_target
WHERE payload = 'single';

SELECT 'ASSERT|insert|multi_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_insert_target
WHERE id BETWEEN 2 AND 4;

SELECT 'ASSERT|insert|copy_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_insert_target
WHERE id >= 101;

SELECT 'ASSERT|insert|sum_amount|' || CAST(SUM(amount) AS VARCHAR(20))
FROM v3inet.v3_insert_target;
