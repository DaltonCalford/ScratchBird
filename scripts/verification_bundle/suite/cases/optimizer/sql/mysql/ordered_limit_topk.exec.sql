SELECT CONCAT('ASSERT|ordered_limit_topk|id=', CAST(id AS CHAR), '|k=', CAST(k AS CHAR))
FROM avp_ordered_limit_topk
ORDER BY k, id
LIMIT 4;
