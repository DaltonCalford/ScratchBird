SELECT 'ASSERT|merge_join_presorted|k=' || CAST(a.k AS VARCHAR(20)) || '|a=' || a.payload || '|b=' || b.payload
FROM avp_merge_join_presorted a
JOIN avp_merge_join_presorted b
  ON a.k = b.k AND a.id < b.id
ORDER BY a.k, a.id, b.id;
