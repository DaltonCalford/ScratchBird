CREATE TABLE vncr_4aab6e_onek(unique1 INTEGER, stringu1 VARCHAR(8));
INSERT INTO vncr_4aab6e_onek VALUES
    (1, 'A'),
    (5, 'B'),
    (12, 'C'),
    (9, 'D');
SELECT 'ASSERT|postgresql_select_ordered_predicate|row|' || CAST(unique1 AS VARCHAR(20))
  FROM vncr_4aab6e_onek
 WHERE unique1 < 10
 ORDER BY unique1;
DROP TABLE vncr_4aab6e_onek;
