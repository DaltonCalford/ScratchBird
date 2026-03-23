EXPLAIN
SELECT a.k, a.payload, b.payload
FROM avp_merge_join_presorted a
JOIN avp_merge_join_presorted b
  ON a.k = b.k AND a.id < b.id
ORDER BY a.k, a.id, b.id;
