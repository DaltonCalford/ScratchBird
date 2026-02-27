DROP TABLE IF EXISTS vt_tx_rollback_visibility;
CREATE TABLE vt_tx_rollback_visibility (
  id INTEGER PRIMARY KEY,
  payload VARCHAR(64)
);
