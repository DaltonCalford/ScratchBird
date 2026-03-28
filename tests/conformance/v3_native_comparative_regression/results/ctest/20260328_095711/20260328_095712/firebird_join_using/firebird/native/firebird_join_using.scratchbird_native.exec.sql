SELECT 'ASSERT|firebird_join_using|row|' || prenom || '|' || name
  FROM vncr_7d8b66_employee
  JOIN vncr_7d8b66_department USING (id_department)
 ORDER BY prenom;
