CREATE TABLE vncr_c70d9f_onek(unique1 INTEGER, stringu1 VARCHAR(8));
INSERT INTO vncr_c70d9f_onek VALUES
    (1, 'A'),
    (5, 'B'),
    (12, 'C'),
    (9, 'D');
SELECT 'ASSERT|postgresql_select_ordered_predicate|row|' || CAST(unique1 AS VARCHAR(20))
  FROM vncr_c70d9f_onek
 WHERE unique1 < 10
 ORDER BY unique1;
DROP TABLE vncr_c70d9f_onek;
