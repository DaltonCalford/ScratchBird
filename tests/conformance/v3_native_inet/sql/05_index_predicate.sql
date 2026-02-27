CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_index_case;

CREATE TABLE v3inet.v3_index_case (
    id INTEGER PRIMARY KEY,
    k INTEGER,
    v VARCHAR(32),
    nullable_col INTEGER
);

INSERT INTO v3inet.v3_index_case (id, k, v, nullable_col) VALUES
    (1, 10, 'u1', 100),
    (2, 10, 'u2', NULL),
    (3, 20, 'u3', 300),
    (4, 20, 'u4', 400),
    (5, 30, 'u5', 500),
    (6, 40, 'u6', NULL),
    (7, 50, 'u7', 700),
    (8, 60, 'u8', 800);

CREATE UNIQUE INDEX idx_v3_index_case_v_unique ON v3inet.v3_index_case(v);
CREATE INDEX idx_v3_index_case_k ON v3inet.v3_index_case(k);
CREATE INDEX idx_v3_index_case_partial ON v3inet.v3_index_case(k) WHERE nullable_col IS NOT NULL;

SELECT 'ASSERT|index|total_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case;

SELECT 'ASSERT|index|eq_k20|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case
WHERE k = 20;

SELECT 'ASSERT|index|neq_k20|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case
WHERE k <> 20;

SELECT 'ASSERT|index|gt_k30|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case
WHERE k > 30;

SELECT 'ASSERT|index|between_20_50|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case
WHERE k BETWEEN 20 AND 50;

SELECT 'ASSERT|index|is_null|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case
WHERE nullable_col IS NULL;

SELECT 'ASSERT|index|in_10_60|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_index_case
WHERE k IN (10, 60);
