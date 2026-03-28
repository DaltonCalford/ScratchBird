CREATE TABLE __VNCR_NS___j1_tbl(i INTEGER, j INTEGER, t TEXT);
CREATE TABLE __VNCR_NS___j2_tbl(i INTEGER, k INTEGER);
INSERT INTO __VNCR_NS___j1_tbl VALUES (1, 4, 'one');
INSERT INTO __VNCR_NS___j1_tbl VALUES (2, 3, 'two');
INSERT INTO __VNCR_NS___j1_tbl VALUES (3, 2, 'three');
INSERT INTO __VNCR_NS___j2_tbl VALUES (1, -1);
INSERT INTO __VNCR_NS___j2_tbl VALUES (2, 2);
INSERT INTO __VNCR_NS___j2_tbl VALUES (3, -3);
SELECT 'ASSERT|postgresql_join_using|row|' || CAST(i AS VARCHAR(20))
  FROM __VNCR_NS___j1_tbl
  JOIN __VNCR_NS___j2_tbl USING (i)
 ORDER BY i;
DROP TABLE __VNCR_NS___j1_tbl;
DROP TABLE __VNCR_NS___j2_tbl;
