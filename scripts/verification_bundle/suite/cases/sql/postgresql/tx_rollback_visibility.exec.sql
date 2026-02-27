BEGIN;
INSERT INTO vt_tx_rollback_visibility (id, payload) VALUES (1, 'rollback_a'), (2, 'rollback_b');
ROLLBACK;
SELECT 'ASSERT|tx_rollback_visibility.rows|' || COUNT(*) FROM vt_tx_rollback_visibility;
