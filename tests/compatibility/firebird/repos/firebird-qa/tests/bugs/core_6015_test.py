#coding:utf-8

"""
ID:          issue-6265
ISSUE:       6265
TITLE:       Segfault when using expression index with complex expression
DESCRIPTION:
  Test creates following objects:
  * table <T> with two columnad, with adding rows;
  * stored proc <P>, which does update one record in the table <T> and returns changed value of column;
  * computed-by index on table <T> which evaluates result of stored proc <P>.

  Procedure can either use:
  1) static PSQL code for updating record, or
  2) change it using ES,
  3) change it using ES+EDS.

  Currently test checks cases "1" and "2" only.
  Check of case "3" if DEFERRED: ISQL will hang and, after it will be interrupted, Firebird process
  keeps database file opened infinitely because of alived EDS connect.
  Discussed with Vlad, letters 17.04.2021 09:52 and (reply from Vlad) 21.04.2021 10:40.

  ::: NB :::
  Code for FB 3.x is separated from 4.x because content of STDERR differs: FB 3.x raises 'lock-conflict'
  for static PSQL code (instead of "Attempt to evaluate index expression recursively").
  Also, error messages differ because CORE-5606 was not backported to FB 3.x.
JIRA:        CORE-6015
FBTEST:      bugs.core_6015
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set term ^;
    create procedure sp_eval_static_psql(a_id int) returns(x_changed int) as
    begin
       suspend;
    end^
    create procedure sp_eval_dynamic_sql(a_id int) returns(x_changed int) as
    begin
       suspend;
    end^
    set term ;^
    commit;

    create table test_static_psql(id int primary key, x int);
    create index test_static_psql_eval on test_static_psql computed by ( ( select x_changed from sp_eval_static_psql( id ) ) );

    create table test_dynamic_sql(id int primary key, x int);
    create index test_dynamic_sql_eval on test_dynamic_sql computed by ( ( select x_changed from sp_eval_dynamic_sql( id ) ) );
    commit;

    insert into test_static_psql(id, x) values(1, 111);
    insert into test_static_psql(id, x) values(2, 222);

    insert into test_dynamic_sql(id, x) values(1, 111);
    insert into test_dynamic_sql(id, x) values(2, 222);
    commit;

    -- connect 'localhost:c:	emp\\c6015.fdb' user 'SYSDBA' password 'masterkey';
    connect '$(DSN)' user 'SYSDBA' password 'masterkey';

    set term ^;
    alter procedure sp_eval_static_psql(a_id int) returns(x_changed int) as
    begin
       update test_static_psql set x = -x order by x rows 1 returning x into x_changed;
       suspend;
    end^

    alter procedure sp_eval_dynamic_sql(a_id int) returns(x_changed int) as
    begin
       execute statement 'update test_dynamic_sql set x = -x order by x rows 1 returning x' into x_changed;
       suspend;
    end^
    set term ;^
    commit;

    set transaction read committed NO wait;

    set bail off;
    set list on;

    -- case-1: check when procedure changes record using STATIC PSQL code:
    -- #######
    select
        t.id as id_case_1
        ,t.x as x_case_1
        ,( select x_changed from sp_eval_static_psql( t.id ) ) as x_changed_1
    from test_static_psql t
    order by id
    ;

    -- case-2: check when procedure changes record using EXECUTE STATEMENT mechanism:
    -- #######
    select
        t.id as id_case_2
        ,t.x as x_case_2
        ,( select x_changed from sp_eval_dynamic_sql( t.id ) ) as x_changed_2
    from test_dynamic_sql t
    order by id
    ;
"""

act = isql_act('db', test_script, substitutions=[('(-)?At procedure .*', '')])

expected_stdout_3x = """
    Statement failed, SQLSTATE = 40001
    lock conflict on no wait transaction
    
    Statement failed, SQLSTATE = 40001
    Attempt to evaluate index expression recursively
    -lock conflict on no wait transaction
"""

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "TEST_STATIC_PSQL_EVAL" on table "TEST_STATIC_PSQL"
    -Attempt to evaluate index expression recursively
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "TEST_DYNAMIC_SQL_EVAL" on table "TEST_DYNAMIC_SQL"
    -Attempt to evaluate index expression recursively
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "PUBLIC"."TEST_STATIC_PSQL_EVAL" on table "PUBLIC"."TEST_STATIC_PSQL"
    -Attempt to evaluate index expression recursively
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "PUBLIC"."TEST_DYNAMIC_SQL_EVAL" on table "PUBLIC"."TEST_DYNAMIC_SQL"
    -Attempt to evaluate index expression recursively
"""


@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
