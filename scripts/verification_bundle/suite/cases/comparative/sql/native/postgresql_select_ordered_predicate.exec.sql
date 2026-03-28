CREATE TABLE __VNCR_NS___onek(unique1 INTEGER, stringu1 VARCHAR(8));
INSERT INTO __VNCR_NS___onek VALUES
    (1, 'A'),
    (5, 'B'),
    (12, 'C'),
    (9, 'D');
SELECT 'ASSERT|postgresql_select_ordered_predicate|row|' || CAST(unique1 AS VARCHAR(20))
  FROM __VNCR_NS___onek
 WHERE unique1 < 10
 ORDER BY unique1;
DROP TABLE __VNCR_NS___onek;
