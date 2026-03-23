RECREATE TABLE avp_point_lookup_exact (
  id INTEGER NOT NULL PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_point_lookup_exact_k ON avp_point_lookup_exact (k);
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (1, 11, 'p011');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (2, 19, 'p019');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (3, 27, 'p027');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (4, 35, 'p035');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (5, 43, 'p043');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (6, 51, 'p051');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (7, 59, 'p059');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (8, 67, 'p067');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (9, 73, 'p073');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (10, 81, 'p081');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (11, 89, 'p089');
INSERT INTO avp_point_lookup_exact (id, k, payload) VALUES (12, 97, 'p097');
COMMIT;
