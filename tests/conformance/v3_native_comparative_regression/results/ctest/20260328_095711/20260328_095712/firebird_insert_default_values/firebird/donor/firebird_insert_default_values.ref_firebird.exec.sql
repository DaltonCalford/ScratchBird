INSERT INTO vncr_2ac7ef_employee DEFAULT VALUES;
SELECT 'ASSERT|firebird_insert_default_values|anon|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_2ac7ef_employee
 WHERE prenom = 'anonymous';
SELECT 'ASSERT|firebird_insert_default_values|male|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_2ac7ef_employee
 WHERE sex = 'M';
