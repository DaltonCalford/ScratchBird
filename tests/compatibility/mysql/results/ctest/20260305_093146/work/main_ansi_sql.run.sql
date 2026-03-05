SET SESSION sql_log_bin = 0;
SET GLOBAL log_bin_trust_function_creators = 1;
-- MySQL Test: ansi
-- Converted from: ansi.test
-- Original path: repos/mysql-server/mysql-test/t/ansi.test

-- Get deafult engine value

-- 
-- Test of ansi mode
-- 



-- Test some functions that works different in ansi mode


-- Test GROUP BY behaviour

CREATE TABLE IF NOT EXISTS t1 (id INT, id2 int);
SELECT id,NULL,1,1.1,'a' FROM t1 GROUP BY id;
-- ONLY_FULL_GROUP_BY is included in ANSI:
drop table t1;


