DROP TABLE IF EXISTS vt_tx_commit_visibility;
CREATE TABLE vt_tx_commit_visibility (
  id INT PRIMARY KEY,
  payload VARCHAR(64)
);
