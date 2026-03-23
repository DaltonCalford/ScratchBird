SELECT 'ASSERT|range_selectivity|count=' || CAST(COUNT(*) AS VARCHAR(20)) || '|sum_id=' || CAST(COALESCE(SUM(id), 0) AS VARCHAR(20))
FROM avp_range_selectivity
WHERE k BETWEEN 25 AND 75;
