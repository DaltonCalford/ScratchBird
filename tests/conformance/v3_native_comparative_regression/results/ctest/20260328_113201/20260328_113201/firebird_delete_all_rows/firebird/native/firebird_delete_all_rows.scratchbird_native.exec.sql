DELETE FROM vncr_a86175_tb;
SELECT 'ASSERT|firebird_delete_all_rows|remaining|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_a86175_tb;
