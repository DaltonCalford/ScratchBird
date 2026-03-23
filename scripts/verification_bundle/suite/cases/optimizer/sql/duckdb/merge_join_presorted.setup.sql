DROP TABLE IF EXISTS avp_merge_join_presorted;
CREATE TABLE avp_merge_join_presorted (
  id INTEGER PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_merge_join_presorted_k ON avp_merge_join_presorted (k);
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES
  (1, 10, 'm10a'),
  (2, 10, 'm10b'),
  (3, 20, 'm20a'),
  (4, 20, 'm20b'),
  (5, 30, 'm30a'),
  (6, 30, 'm30b');
