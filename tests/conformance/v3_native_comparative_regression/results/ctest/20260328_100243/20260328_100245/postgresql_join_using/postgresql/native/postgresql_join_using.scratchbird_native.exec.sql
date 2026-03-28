CREATE TABLE vncr_7799dc_j1_tbl(i INTEGER, j INTEGER, t TEXT);
CREATE TABLE vncr_7799dc_j2_tbl(i INTEGER, k INTEGER);
INSERT INTO vncr_7799dc_j1_tbl VALUES (1, 4, 'one');
INSERT INTO vncr_7799dc_j1_tbl VALUES (2, 3, 'two');
INSERT INTO vncr_7799dc_j1_tbl VALUES (3, 2, 'three');
INSERT INTO vncr_7799dc_j2_tbl VALUES (1, -1);
INSERT INTO vncr_7799dc_j2_tbl VALUES (2, 2);
INSERT INTO vncr_7799dc_j2_tbl VALUES (3, -3);
SELECT 'ASSERT|postgresql_join_using|row|' || CAST(i AS VARCHAR(20))
  FROM vncr_7799dc_j1_tbl
  JOIN vncr_7799dc_j2_tbl USING (i)
 ORDER BY i;
DROP TABLE vncr_7799dc_j1_tbl;
DROP TABLE vncr_7799dc_j2_tbl;
