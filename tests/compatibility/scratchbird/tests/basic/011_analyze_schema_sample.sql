CREATE SCHEMA analyze_schema;

CREATE TABLE analyze_schema.sample_table (
    id INT PRIMARY KEY,
    value INT
);

INSERT INTO analyze_schema.sample_table (id, value) VALUES
    (1, 10),
    (2, 20),
    (3, 30);

ANALYZE analyze_schema.sample_table;
ANALYZE analyze_schema.sample_table SAMPLE 0;
ANALYZE analyze_schema.sample_table SAMPLE 1;

DROP TABLE analyze_schema.sample_table;
DROP SCHEMA analyze_schema;
