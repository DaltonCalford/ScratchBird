SELECT 'ASSERT|firebird_avg_single_row|value|' || CAST(CAST(SUM(id) / COUNT(*) AS INTEGER) AS VARCHAR(20))
  FROM vncr_ab44ce_test;
