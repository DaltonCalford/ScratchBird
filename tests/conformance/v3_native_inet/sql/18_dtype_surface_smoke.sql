CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_dtype_case;

CREATE TABLE v3inet.v3_dtype_case (
    id INTEGER PRIMARY KEY,
    c_small SMALLINT,
    c_int INTEGER,
    c_big BIGINT,
    c_bool BOOLEAN,
    c_dec DECIMAL(18,4),
    c_char CHAR(8),
    c_varchar VARCHAR(32),
    c_text TEXT,
    c_jsonb JSONB,
    c_date DATE,
    c_ts TIMESTAMP,
    c_arr_int INTEGER[]
);

INSERT INTO v3inet.v3_dtype_case (
    id,
    c_small,
    c_int,
    c_big,
    c_bool,
    c_dec,
    c_char,
    c_varchar,
    c_text,
    c_jsonb,
    c_date,
    c_ts,
    c_arr_int
) VALUES (
    1,
    CAST(7 AS SMALLINT),
    CAST(777 AS INTEGER),
    CAST(7777 AS BIGINT),
    TRUE,
    CAST(12.3400 AS DECIMAL(18,4)),
    CAST('abc' AS CHAR(8)),
    'varchar_value',
    'text_value',
    '{"kind":"jsonb"}'::JSONB,
    DATE '2024-01-01',
    TIMESTAMP '2024-01-01 01:02:03',
    ARRAY[1,2,3]
);

CREATE INDEX idx_v3_dtype_jsonb_gin ON v3inet.v3_dtype_case USING GIN (c_jsonb);
CREATE INDEX idx_v3_dtype_int ON v3inet.v3_dtype_case(c_int);

SELECT 'ASSERT|dtype|row_count|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_dtype_case;

SELECT 'ASSERT|dtype|jsonb_kind|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_dtype_case
WHERE c_jsonb @> '{"kind":"jsonb"}'::JSONB;

SELECT 'ASSERT|dtype|column_count|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.columns
WHERE table_schema = 'v3inet'
  AND table_name = 'v3_dtype_case';
