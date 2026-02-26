CREATE DATABASE IF NOT EXISTS `compat_mysql_main_auto_increment_sql`;
DROP DATABASE IF EXISTS `test`;
DROP DATABASE IF EXISTS `db1`;
DROP DATABASE IF EXISTS `db2`;
DROP DATABASE IF EXISTS `db3`;
DROP DATABASE IF EXISTS `mysqltest`;
CREATE DATABASE `test`;
USE `compat_mysql_main_auto_increment_sql`;
SET SESSION sql_log_bin = 0;
SET GLOBAL log_bin_trust_function_creators = 1;
SET SESSION sql_mode = '';
-- MySQL Test: auto_increment
-- Converted from: auto_increment.test
-- Original path: repos/mysql-server/mysql-test/t/auto_increment.test

-- This testcase is an auto increment feature specific, to Myisam
-- All tests are required to run with Myisam
-- Hence MTR starts mysqld with MyISAM as default

-- 
-- Test of auto_increment;  The test for BDB tables is in bdb.test
-- 
drop table if exists t1;
drop table if exists t2;
SET SQL_WARNINGS=1;

CREATE TABLE IF NOT EXISTS t1 (a int not null auto_increment,b int, primary key (a)) engine=myisam auto_increment=3;
insert into t1 values (1,1),(NULL,3),(NULL,4);
delete from t1 where a=4;
insert into t1 values (NULL,5),(NULL,6);
select * from t1;
delete from t1 where a=6;
-- show table status like "t1";
replace t1 values (3,1);
ALTER TABLE t1 add c int;
replace t1 values (3,3,3);
insert into t1 values (NULL,7,7);
update t1 set a=8,b=b+1,c=c+1 where a=7;
insert into t1 values (NULL,9,9);
select * from t1;
drop table t1;

CREATE TABLE IF NOT EXISTS t1 (
  skey tinyint unsigned NOT NULL auto_increment PRIMARY KEY,
  sval char(20)
);
insert into t1 values (NULL, "hello");
insert into t1 values (NULL, "hey");
select * from t1;
select _rowid,t1._rowid,skey,sval from t1;
drop table t1;

-- 
-- Test auto_increment on sub key
-- 

CREATE TABLE IF NOT EXISTS t1 (ordid int(8) not null auto_increment, ord  varchar(50) not null, primary key (ordid), index(ord,ordid)); 
insert into t1 (ordid,ord) values (NULL,'sdj'),(NULL,'sdj');
select * from t1;
drop table t1;



-- 
-- Test negative values (Bug #1366)
-- 

CREATE TABLE IF NOT EXISTS t1 (a int not null auto_increment primary key);
insert into t1 values (NULL);
insert into t1 values (-1);
select last_insert_id();
insert into t1 values (NULL);
select * from t1;
drop table t1;

CREATE TABLE IF NOT EXISTS t1 (a int not null auto_increment primary key) /*!40102 engine=heap */;
insert into t1 values (NULL);
insert into t1 values (-1);
select last_insert_id();
insert into t1 values (NULL);
select * from t1;
drop table t1;
-- 
-- last_insert_id() madness
-- 
CREATE TABLE IF NOT EXISTS t1 (i tinyint unsigned not null auto_increment primary key);
insert into t1 set i = 254;
insert into t1 set i = null;
select last_insert_id();
explain select last_insert_id();
select last_insert_id();
select last_insert_id();
drop table t1;

CREATE TABLE IF NOT EXISTS t1 (i tinyint unsigned not null auto_increment, key (i));
insert into t1 set i = 254;
insert into t1 set i = null;
select last_insert_id();
insert into t1 set i = null;
select last_insert_id();
drop table t1;

CREATE TABLE IF NOT EXISTS t1 (i tinyint unsigned not null auto_increment primary key, b int, unique (b));
insert into t1 values (NULL, 10);
select last_insert_id();
insert into t1 values (NULL, 15);
select last_insert_id();
select last_insert_id();

drop table t1;

CREATE TABLE IF NOT EXISTS t1(a int auto_increment,b int null,primary key(a));
insert into t1(a,b)values(NULL,1);
insert into t1(a,b)values(200,2);
insert into t1(a,b)values(0,3);
insert into t1(b)values(4);
insert into t1(b)values(5);
insert into t1(b)values(6);
insert into t1(b)values(7);
select * from t1 order by b;
alter table t1 modify b mediumint;
select * from t1 order by b;
CREATE TABLE IF NOT EXISTS t2 (a int);
insert t2 values (1),(2);
alter table t2 add b int auto_increment primary key;
select * from t2;
drop table t2;
delete from t1 where a=0;
update t1 set a=0 where b=5;
select * from t1 order by b;
delete from t1 where a=0;
update t1 set a=300 where b=7;
insert into t1(a,b)values(NULL,8);
insert into t1(a,b)values(400,9);
insert into t1(a,b)values(0,10);
insert into t1(b)values(11);
insert into t1(b)values(12);
insert into t1(b)values(13);
insert into t1(b)values(14);
select * from t1 order by b;
delete from t1 where a=0;
update t1 set a=0 where b=12;
select * from t1 order by b;
delete from t1 where a=0;
update t1 set a=500 where b=14;
select * from t1 order by b;
drop table t1;

-- 
-- Test of behavior of ALTER TABLE when coulmn containing NULL or zeroes is
-- converted to AUTO_INCREMENT column
-- 
CREATE TABLE IF NOT EXISTS t1 (a bigint);
insert into t1 values (1), (2), (3), (NULL), (NULL);
alter table t1 modify a bigint not null auto_increment primary key; 
select * from t1;
drop table t1;

CREATE TABLE IF NOT EXISTS t1 (a bigint);
insert into t1 values (1), (2), (3), (0), (0);
alter table t1 modify a bigint not null auto_increment primary key; 
select * from t1;
drop table t1;

-- We still should be able to preserve zero in NO_AUTO_VALUE_ON_ZERO mode
CREATE TABLE IF NOT EXISTS t1 (a bigint);
insert into t1 values (0), (1), (2), (3);
alter table t1 modify a bigint not null auto_increment primary key; 
select * from t1;
drop table t1;

-- It also sensible to preserve zeroes if we are converting auto_increment
-- column to auto_increment column (or not touching it at all, which is more
-- common case probably)
CREATE TABLE IF NOT EXISTS t1 (a int auto_increment primary key , b int null);
insert into t1 values (0,1),(1,2),(2,3);
select * from t1;
alter table t1 modify b varchar(255);
insert into t1 values (0,4);
select * from t1;
drop table t1;

-- 
-- BUG #10045: Problem with composite AUTO_INCREMENT + BLOB key

CREATE TABLE IF NOT EXISTS t1 ( a INT AUTO_INCREMENT, b BLOB, PRIMARY KEY (a,b(10)));
INSERT INTO t1 (b) VALUES ('aaaa');
CHECK TABLE t1;
INSERT INTO t1 (b) VALUES ('');
CHECK TABLE t1;
INSERT INTO t1 (b) VALUES ('bbbb');
CHECK TABLE t1;
DROP TABLE IF EXISTS t1;

-- BUG #19025:

CREATE TABLE IF NOT EXISTS `t1` (
    t1_name VARCHAR(255) DEFAULT NULL,
    t1_id INT(10) UNSIGNED NOT NULL AUTO_INCREMENT,
    KEY (t1_name),
    PRIMARY KEY (t1_id)
) charset latin1 AUTO_INCREMENT = 1000;

INSERT INTO t1 (t1_name) VALUES('MySQL');
INSERT INTO t1 (t1_name) VALUES('MySQL');
INSERT INTO t1 (t1_name) VALUES('MySQL');

SELECT * from t1;

SHOW CREATE TABLE `t1`;

DROP TABLE `t1`;

-- 
-- Bug #6880: LAST_INSERT_ID() within a statement
-- 

CREATE TABLE IF NOT EXISTS t1(a int not null auto_increment primary key);              
CREATE TABLE IF NOT EXISTS t2(a int not null auto_increment primary key, t1a int);     
insert into t1 values(NULL);
insert into t2 values (NULL, LAST_INSERT_ID()), (NULL, LAST_INSERT_ID());
insert into t1 values (NULL);
insert into t2 values (NULL, LAST_INSERT_ID()), (NULL, LAST_INSERT_ID()),
(NULL, LAST_INSERT_ID());
insert into t1 values (NULL);                                            
insert into t2 values (NULL, LAST_INSERT_ID()), (NULL, LAST_INSERT_ID()),
(NULL, LAST_INSERT_ID()), (NULL, LAST_INSERT_ID());
select * from t2;
drop table t1, t2;


-- 
-- Bug #11080 & #11005  Multi-row REPLACE fails on a duplicate key error
-- 

CREATE TABLE IF NOT EXISTS t1 ( `a` int(11) NOT NULL auto_increment, `b` int(11) default NULL,PRIMARY KEY  (`a`),UNIQUE KEY `b` (`b`));
insert into t1 (b) values (1);
replace into t1 (b) values (2), (1), (3);
select * from t1;
truncate table t1;
insert into t1 (b) values (1);
replace into t1 (b) values (2);
replace into t1 (b) values (1);
replace into t1 (b) values (3);
select * from t1;
drop table t1;

CREATE TABLE IF NOT EXISTS t1 (rowid int not null auto_increment, val int not null,primary
key (rowid), unique(val));
replace into t1 (val) values ('1'),('2');
replace into t1 (val) values ('1'),('2');
select * from t1;
drop table t1;

-- 
-- Test that update changes internal auto-increment value
-- 

CREATE TABLE IF NOT EXISTS t1 (a int not null auto_increment primary key, val int);
insert into t1 (val) values (1);
update t1 set a=2 where a=1;
insert into t1 (val) values (1);
select * from t1;
drop table t1;

-- 
-- Test key duplications with auto-increment in ALTER TABLE
-- bug #14573
-- 
CREATE TABLE IF NOT EXISTS t1 (t1 INT(10) PRIMARY KEY, t2 INT(10));
INSERT INTO t1 VALUES(0, 0);
INSERT INTO t1 VALUES(1, 1);
DROP TABLE t1;

-- Test of REPLACE when it does INSERT+DELETE and not UPDATE:
-- see if it sets LAST_INSERT_ID() ok
CREATE TABLE IF NOT EXISTS t1 (a int primary key auto_increment, b int, c int, d timestamp default current_timestamp, unique(b),unique(c));
insert into t1 values(null,1,1,now());
insert into t1 values(null,0,0,null);
-- this will delete two rows
replace into t1 values(null,1,0,null);
select last_insert_id();
drop table t1;

-- Test of REPLACE when it does a INSERT+DELETE for all the conflicting rows
-- (i.e.) when there are three rows conflicting in unique key columns with
-- a row that is being inserted, all the three rows will be deleted and then
-- the new rows will be inserted.
CREATE TABLE IF NOT EXISTS t1 (a int primary key auto_increment, b int, c int, e int, d timestamp default current_timestamp, unique(b),unique(c),unique(e));
insert into t1 values(null,1,1,1,now());
insert into t1 values(null,0,0,0,null);
replace into t1 values(null,1,0,2,null);
select last_insert_id();
drop table t1;

CREATE TABLE IF NOT EXISTS t1 ( a INT );
INSERT INTO t1 VALUES (1), (1);

CREATE TABLE IF NOT EXISTS t2 ( a INT AUTO_INCREMENT KEY );

UPDATE t2 SET a = 2;

SELECT a FROM t2;

DROP TABLE t1, t2;


CREATE TABLE IF NOT EXISTS t1 (c1 BIGINT UNSIGNED AUTO_INCREMENT, PRIMARY KEY(c1)) engine=MyISAM;
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES (18446744073709551601);

SET @@SESSION.AUTO_INCREMENT_INCREMENT=10;

SELECT @@SESSION.AUTO_INCREMENT_OFFSET;
SELECT * FROM t1;

SET @@SESSION.AUTO_INCREMENT_INCREMENT=default;
SET @@SESSION.AUTO_INCREMENT_OFFSET=default;

DROP TABLE t1;



CREATE TABLE IF NOT EXISTS t1 (pk INT AUTO_INCREMENT, PRIMARY KEY (pk));
-- This triggered the assert
INSERT INTO t1 VALUES (NULL), (-1), (NULL);
SELECT * FROM t1;
DROP TABLE t1;

-- Check that that true overflow still gives error
CREATE TABLE IF NOT EXISTS t1 (pk BIGINT UNSIGNED AUTO_INCREMENT, PRIMARY KEY (pk));
SELECT * FROM t1;
DROP TABLE t1;

