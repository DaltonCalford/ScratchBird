DROP TABLE IF EXISTS vt_index_lookup_predicates;
CREATE TABLE vt_index_lookup_predicates (
  id INT PRIMARY KEY,
  k INT NOT NULL,
  payload VARCHAR(64)
);
CREATE INDEX idx_vt_index_lookup_predicates_k ON vt_index_lookup_predicates (k);
INSERT INTO vt_index_lookup_predicates (id, k, payload) VALUES
  (1, 10, 'a'),
  (2, 20, 'b'),
  (3, 20, 'c'),
  (4, 30, 'd');
