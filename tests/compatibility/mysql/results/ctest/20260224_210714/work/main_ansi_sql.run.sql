CREATE DATABASE IF NOT EXISTS `compat_mysql_main_ansi_sql`;
DROP DATABASE IF EXISTS `test`;
DROP DATABASE IF EXISTS `db1`;
DROP DATABASE IF EXISTS `db2`;
DROP DATABASE IF EXISTS `db3`;
DROP DATABASE IF EXISTS `mysqltest`;
CREATE DATABASE `test`;
USE `compat_mysql_main_ansi_sql`;
SET SESSION sql_log_bin = 0;
SET GLOBAL log_bin_trust_function_creators = 1;
SET SESSION sql_mode = '';
-- MySQL Test: ansi
-- Converted from: ansi.test
-- Original path: repos/mysql-server/mysql-test/t/ansi.test

-- Get deafult engine value

-- 
-- Test of ansi mode
-- 

drop table if exists t1;

select @@sql_mode;

-- Test some functions that works different in ansi mode

SELECT 'A' || 'B';

-- Test GROUP BY behaviour

CREATE TABLE IF NOT EXISTS t1 (id INT, id2 int);
SELECT id,NULL,1,1.1,'a' FROM t1 GROUP BY id;
-- ONLY_FULL_GROUP_BY is included in ANSI:
drop table t1;


