SELECT 'ASSERT|index_lookup_predicates.eq|' || COUNT(*)
FROM vt_index_lookup_predicates WHERE k = 20;
SELECT 'ASSERT|index_lookup_predicates.neq|' || COUNT(*)
FROM vt_index_lookup_predicates WHERE k <> 20;
