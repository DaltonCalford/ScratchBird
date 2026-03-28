SELECT 'ASSERT|firebird_join_using|row|benoit|' || name
  FROM vncr_d8252a_employee e
  JOIN vncr_d8252a_department d ON e.id_department = d.id_department
 WHERE e.prenom = 'benoit';
SELECT 'ASSERT|firebird_join_using|row|tom|' || name
  FROM vncr_d8252a_employee e
  JOIN vncr_d8252a_department d ON e.id_department = d.id_department
 WHERE e.prenom = 'tom';
