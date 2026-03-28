SELECT 'ASSERT|firebird_avg_single_row|value|' ||
       CAST((SELECT AVG(id) FROM vncr_2f8a54_test) AS VARCHAR(20))
  FROM RDB$DATABASE;
