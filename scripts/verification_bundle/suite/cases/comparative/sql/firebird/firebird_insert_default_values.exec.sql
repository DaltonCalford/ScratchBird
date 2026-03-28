INSERT INTO __VNCR_NS___employee DEFAULT VALUES;
SELECT 'ASSERT|firebird_insert_default_values|anon|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM __VNCR_NS___employee
 WHERE prenom = 'anonymous';
SELECT 'ASSERT|firebird_insert_default_values|male|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM __VNCR_NS___employee
 WHERE sex = 'M';
