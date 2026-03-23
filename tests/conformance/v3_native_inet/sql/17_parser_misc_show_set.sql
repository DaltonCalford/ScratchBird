CREATE SCHEMA IF NOT EXISTS v3inet;

DROP TABLE IF EXISTS v3inet.v3_misc_table;

CREATE TABLE v3inet.v3_misc_table (
  id INTEGER PRIMARY KEY,
  payload VARCHAR(32)
);

INSERT INTO v3inet.v3_misc_table (id, payload) VALUES
  (1, 'm1'),
  (2, 'm2'),
  (3, 'm3');

SET search_path TO v3inet, users.public;
SHOW search_path;
SHOW server_version;
SHOW TABLE v3inet.v3_misc_table;

EXPLAIN SELECT id, payload FROM v3inet.v3_misc_table WHERE id > 1;

SELECT 'ASSERT|misc|rows_gt_1|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3inet.v3_misc_table
WHERE id > 1;

SELECT 'ASSERT|misc|survived|' || '1';
