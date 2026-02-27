UPDATE vt_dml_update_delete SET score = score + 5 WHERE flag = 1;
DELETE FROM vt_dml_update_delete WHERE id = 2;
SELECT 'ASSERT|dml_update_delete.rows|' || COUNT(*) FROM vt_dml_update_delete;
SELECT 'ASSERT|dml_update_delete.sum_score|' || COALESCE(SUM(score), 0) FROM vt_dml_update_delete;
