DROP TABLE IF EXISTS avp_point_lookup_exact;
CREATE TABLE avp_point_lookup_exact (
  id INTEGER PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_point_lookup_exact_k ON avp_point_lookup_exact (k);
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES
  (1, 11, 'p011'),
  (2, 19, 'p019'),
  (3, 27, 'p027'),
  (4, 35, 'p035'),
  (5, 43, 'p043'),
  (6, 51, 'p051'),
  (7, 59, 'p059'),
  (8, 67, 'p067'),
  (9, 73, 'p073'),
  (10, 81, 'p081'),
  (11, 89, 'p089'),
  (12, 97, 'p097');
