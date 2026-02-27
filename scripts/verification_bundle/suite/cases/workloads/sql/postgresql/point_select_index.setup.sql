DROP TABLE IF EXISTS vw_point_select_index;
CREATE TABLE vw_point_select_index (
  id INTEGER PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32)
);
CREATE INDEX idx_vw_point_select_index_k ON vw_point_select_index (k);
INSERT INTO vw_point_select_index (id, k, payload) VALUES
  (1, 10, 'a'),(2, 20, 'b'),(3, 30, 'c'),(4, 40, 'd'),
  (5, 10, 'e'),(6, 20, 'f'),(7, 30, 'g'),(8, 40, 'h'),
  (9, 10, 'i'),(10, 20, 'j'),(11, 30, 'k'),(12, 40, 'l');
