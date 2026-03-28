SELECT 'ASSERT|firebird_join_using|row|benoit|' || name
  FROM vncr_bed9e5_employee
  JOIN vncr_bed9e5_department USING (id_department)
 WHERE prenom = 'benoit';
SELECT 'ASSERT|firebird_join_using|row|tom|' || name
  FROM vncr_bed9e5_employee
  JOIN vncr_bed9e5_department USING (id_department)
 WHERE prenom = 'tom';
