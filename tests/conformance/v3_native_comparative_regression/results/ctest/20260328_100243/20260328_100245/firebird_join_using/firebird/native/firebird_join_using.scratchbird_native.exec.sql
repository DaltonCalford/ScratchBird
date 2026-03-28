SELECT 'ASSERT|firebird_join_using|row|benoit|' || name
  FROM vncr_6050d1_employee e
  JOIN vncr_6050d1_department d ON e.id_department = d.id_department
 WHERE e.prenom = 'benoit';
SELECT 'ASSERT|firebird_join_using|row|tom|' || name
  FROM vncr_6050d1_employee e
  JOIN vncr_6050d1_department d ON e.id_department = d.id_department
 WHERE e.prenom = 'tom';
