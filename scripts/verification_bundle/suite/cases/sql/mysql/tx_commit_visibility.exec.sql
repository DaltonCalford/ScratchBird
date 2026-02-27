START TRANSACTION;
INSERT INTO vt_tx_commit_visibility (id, payload) VALUES (1, 'commit_a'), (2, 'commit_b');
COMMIT;
SELECT CONCAT('ASSERT|tx_commit_visibility.rows|', COUNT(*)) FROM vt_tx_commit_visibility;
