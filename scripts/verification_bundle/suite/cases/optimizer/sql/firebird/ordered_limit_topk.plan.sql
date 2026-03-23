SET PLAN ON;
SELECT id, k FROM avp_ordered_limit_topk ORDER BY k, id ROWS 4;
SET PLAN OFF;
