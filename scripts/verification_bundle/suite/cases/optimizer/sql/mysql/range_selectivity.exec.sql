SELECT CONCAT('ASSERT|range_selectivity|count=', CAST(COUNT(*) AS CHAR), '|sum_id=', CAST(COALESCE(SUM(id), 0) AS CHAR))
FROM avp_range_selectivity
WHERE k BETWEEN 25 AND 75;
