SELECT 'ASSERT|expression_lookup|id=' || CAST(id AS VARCHAR(20)) || '|payload=' || payload
FROM avp_expression_lookup
WHERE LOWER(name) = 'charlie'
ORDER BY id;
