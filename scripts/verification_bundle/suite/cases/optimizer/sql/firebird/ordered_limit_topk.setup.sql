RECREATE TABLE avp_ordered_limit_topk (
  id INTEGER NOT NULL PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_ordered_limit_topk_k ON avp_ordered_limit_topk (k);
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (1, 90, 'k090');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (2, 40, 'k040');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (3, 10, 'k010a');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (4, 20, 'k020a');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (5, 10, 'k010b');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (6, 30, 'k030');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (7, 50, 'k050');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (8, 10, 'k010c');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (9, 60, 'k060');
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES (10, 20, 'k020b');
COMMIT;
