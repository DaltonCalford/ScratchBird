SELECT 'ASSERT|dml_join_aggregate.join_rows|' || COUNT(*)
FROM vt_dml_join_parent p
JOIN vt_dml_join_child c ON c.parent_id = p.id;
SELECT 'ASSERT|dml_join_aggregate.sum_metric|' || COALESCE(SUM(c.metric), 0)
FROM vt_dml_join_parent p
JOIN vt_dml_join_child c ON c.parent_id = p.id;
