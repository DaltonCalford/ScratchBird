INSERT INTO vncr_2799bf_employee DEFAULT VALUES;
SELECT 'ASSERT|firebird_insert_default_values|anon|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_2799bf_employee
 WHERE prenom = 'anonymous';
SELECT 'ASSERT|firebird_insert_default_values|male|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_2799bf_employee
 WHERE sex = 'M';
