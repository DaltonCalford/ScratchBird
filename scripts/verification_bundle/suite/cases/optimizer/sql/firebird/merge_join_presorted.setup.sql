RECREATE TABLE avp_merge_join_presorted (
  id INTEGER NOT NULL PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_merge_join_presorted_k ON avp_merge_join_presorted (k);
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES (1, 10, 'm10a');
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES (2, 10, 'm10b');
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES (3, 20, 'm20a');
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES (4, 20, 'm20b');
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES (5, 30, 'm30a');
INSERT INTO avp_merge_join_presorted (id, k, payload) VALUES (6, 30, 'm30b');
COMMIT;
