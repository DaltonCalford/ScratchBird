SELECT 'ASSERT|point_lookup_exact|count=' || CAST(COUNT(*) AS VARCHAR(20)) || '|payload=' || COALESCE(MIN(payload), 'NULL')
FROM avp_point_lookup_exact
WHERE k = 73;
