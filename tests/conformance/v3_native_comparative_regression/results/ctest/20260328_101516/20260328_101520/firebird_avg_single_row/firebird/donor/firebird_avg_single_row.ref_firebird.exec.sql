SELECT 'ASSERT|firebird_avg_single_row|value|' ||
       CAST(CAST(AVG(id) AS INTEGER) AS VARCHAR(20))
  FROM vncr_00bc42_test;
