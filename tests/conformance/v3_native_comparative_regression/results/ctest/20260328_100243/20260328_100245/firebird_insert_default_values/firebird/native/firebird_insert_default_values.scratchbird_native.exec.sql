INSERT INTO vncr_ea1933_employee DEFAULT VALUES;
SELECT 'ASSERT|firebird_insert_default_values|anon|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_ea1933_employee
 WHERE prenom = 'anonymous';
SELECT 'ASSERT|firebird_insert_default_values|male|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_ea1933_employee
 WHERE sex = 'M';
