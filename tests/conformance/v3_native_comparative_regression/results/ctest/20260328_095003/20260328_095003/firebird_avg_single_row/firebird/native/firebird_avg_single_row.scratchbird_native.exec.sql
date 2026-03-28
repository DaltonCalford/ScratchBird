CREATE TABLE vncr_6ba7b3_test(id INTEGER NOT NULL);
INSERT INTO vncr_6ba7b3_test VALUES (5);
SELECT 'ASSERT|firebird_avg_single_row|value|' || CAST(CAST(AVG(id) AS INTEGER) AS VARCHAR(20))
  FROM vncr_6ba7b3_test;
DROP TABLE vncr_6ba7b3_test;
