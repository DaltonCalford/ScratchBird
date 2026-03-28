CREATE TABLE vncr_ece440_test(id INTEGER);
SELECT 'ASSERT|firebird_count_empty|value|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_ece440_test;
DROP TABLE vncr_ece440_test;
