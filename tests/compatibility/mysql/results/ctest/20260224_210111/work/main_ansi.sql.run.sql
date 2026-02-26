CREATE DATABASE IF NOT EXISTS `compat_mysql_main_ansi.sql`;
DROP DATABASE IF EXISTS `test`;
CREATE DATABASE `test`;
USE `compat_mysql_main_ansi.sql`;
SET SESSION sql_log_bin = 0;
SET GLOBAL log_bin_trust_function_creators = 1;
SET SESSION sql_mode = REPLACE(@@SESSION.sql_mode, 'ONLY_FULL_GROUP_BY', '');
-- MySQL Test: ansi
-- Converted from: ansi.test
-- Original path: repos/mysql-server/mysql-test/t/ansi.test

-- Get deafult engine value

-- 
-- Test of ansi mode
-- 

drop table if exists t1;

set @@sql_mode="ANSI";
select @@sql_mode;

-- Test some functions that works different in ansi mode

SELECT 'A' || 'B';

-- Test GROUP BY behaviour

CREATE TABLE t1 (id INT, id2 int);
SELECT id,NULL,1,1.1,'a' FROM t1 GROUP BY id;
-- ONLY_FULL_GROUP_BY is included in ANSI:
drop table t1;

SET @@SQL_MODE="";

