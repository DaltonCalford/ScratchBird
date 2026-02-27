INSERT INTO vt_dml_insert_multirow (id, grp, payload) VALUES (1, 1, 'r1');
INSERT INTO vt_dml_insert_multirow (id, grp, payload) VALUES
  (2, 1, 'r2'),
  (3, 2, 'r3'),
  (4, 2, 'r4');
SELECT 'ASSERT|dml_insert_multirow.rows|' || COUNT(*) FROM vt_dml_insert_multirow;
SELECT 'ASSERT|dml_insert_multirow.grp2|' || COUNT(*) FROM vt_dml_insert_multirow WHERE grp = 2;
