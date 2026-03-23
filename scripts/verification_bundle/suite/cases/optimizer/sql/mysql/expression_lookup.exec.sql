SELECT CONCAT('ASSERT|expression_lookup|id=', CAST(id AS CHAR), '|payload=', payload)
FROM avp_expression_lookup
WHERE LOWER(name) = 'charlie'
ORDER BY id;
