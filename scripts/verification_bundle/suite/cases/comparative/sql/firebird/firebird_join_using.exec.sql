SELECT 'ASSERT|firebird_join_using|row|benoit|' || name
  FROM __VNCR_NS___employee e
  JOIN __VNCR_NS___department d ON e.id_department = d.id_department
 WHERE e.prenom = 'benoit';
SELECT 'ASSERT|firebird_join_using|row|tom|' || name
  FROM __VNCR_NS___employee e
  JOIN __VNCR_NS___department d ON e.id_department = d.id_department
 WHERE e.prenom = 'tom';
