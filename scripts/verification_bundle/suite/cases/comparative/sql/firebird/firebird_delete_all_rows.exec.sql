DELETE FROM __VNCR_NS___tb;
SELECT 'ASSERT|firebird_delete_all_rows|remaining|' || CAST(COUNT(*) AS VARCHAR(20)) FROM __VNCR_NS___tb;
