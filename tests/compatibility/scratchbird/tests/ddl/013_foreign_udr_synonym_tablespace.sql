-- DDL lifecycle objects: tablespace, group, foreign table, synonym, UDR

CREATE TABLESPACE ts_test LOCATION '/tmp/scratchbird_ts_test.sbts'
    AUTOEXTEND ON AUTOEXTEND_SIZE 10 MAXSIZE 100 PREALLOC 5;
ALTER TABLESPACE ts_test AUTOEXTEND OFF;
DROP TABLESPACE ts_test FORCE;

CREATE GROUP devs;
DROP GROUP devs;

CREATE SERVER reporting_db
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'localhost', port '5432', dbname 'analytics');

CREATE USER MAPPING FOR CURRENT_USER
    SERVER reporting_db
    OPTIONS (user 'remote_user', password 'secret');

CREATE FOREIGN TABLE IF NOT EXISTS local_sales (
    sale_id UUID,
    amount DECIMAL(18,2)
) SERVER reporting_db
  OPTIONS (schema_name 'public', table_name 'sales');

DROP FOREIGN TABLE IF EXISTS local_sales;
DROP USER MAPPING FOR CURRENT_USER SERVER reporting_db;
DROP SERVER reporting_db CASCADE;

CREATE TABLE ddl_synonym_base (id INT);
CREATE SYNONYM ddl_synonym_alias FOR TABLE ddl_synonym_base;
DROP SYNONYM ddl_synonym_alias;
DROP TABLE ddl_synonym_base;

CREATE UDR FUNCTION udr_test
    AS '/tmp/libscratchbird_udr.so'
    ENTRY 'udr_entry'
    SIGNATURE 'udr_test()';

DROP UDR udr_test;
