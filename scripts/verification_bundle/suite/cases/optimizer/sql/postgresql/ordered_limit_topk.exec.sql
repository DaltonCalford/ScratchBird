SELECT 'ASSERT|ordered_limit_topk|id=' || CAST(id AS VARCHAR(20)) || '|k=' || CAST(k AS VARCHAR(20))
FROM avp_ordered_limit_topk
ORDER BY k, id
LIMIT 4;
