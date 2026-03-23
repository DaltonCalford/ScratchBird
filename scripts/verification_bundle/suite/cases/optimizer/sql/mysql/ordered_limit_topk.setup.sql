DROP TABLE IF EXISTS avp_ordered_limit_topk;
CREATE TABLE avp_ordered_limit_topk (
  id INTEGER PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_ordered_limit_topk_k ON avp_ordered_limit_topk (k);
INSERT INTO avp_ordered_limit_topk (id, k, payload) VALUES
  (1, 90, 'k090'),
  (2, 40, 'k040'),
  (3, 10, 'k010a'),
  (4, 20, 'k020a'),
  (5, 10, 'k010b'),
  (6, 30, 'k030'),
  (7, 50, 'k050'),
  (8, 10, 'k010c'),
  (9, 60, 'k060'),
  (10, 20, 'k020b');
