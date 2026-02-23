#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8666
TITLE:       Crash after calling incorrectly parametrized request
NOTES:
    [24.07.2025] pzotov
    Confirmed crash on 6.0.0.1052-c6658eb
    Checked on 6.0.0.1061-44da3ac; 5.0.3.1686-1f2fcff; 4.0.6.3223-cb61311 (intermediate snapshots).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail OFF;
    set list on;

    set term ^;
    execute block 
    as 
         declare sql_stmt    varchar(200);
         declare dept_id     numeric(2) = 50;
         declare dept_name   type of column dept.name  = 'personnel';
         declare location    type of column dept.location default 'dallas';
    begin
        sql_stmt = 'insert into dept values (:a, :b, :c)';
        execute statement (:sql_stmt) (:dept_id, :dept_name, :location);
    end
    ^

    execute block 
    as 
         declare sql_stmt    varchar(200);
         declare dept_id     numeric(2) = 50;
         declare dept_name   type of column dept.name  = 'personnel';
         declare location    type of column dept.location default 'dallas';
    begin
        sql_stmt = 'insert into dept values (:a, :b, :c)';
        execute statement (:sql_stmt) (a := :dept_id, b :=  :dept_name, c :=  :location);
    end
    ^

    execute block 
    as 
         declare sql_stmt    varchar(200);
         declare dept_id     numeric(2) = 50;
         declare dept_name   type of column dept.name  = 'personnel';
         declare location    type of column dept.location default 'dallas';
    begin
        sql_stmt = 'insert into dept values (:a, :b, :c)';
        execute statement (:sql_stmt) (:dept_id, :dept_name, :location);
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -607
    -Invalid command
    -column NAME does not exist in table/view DEPT

    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -607
    -Invalid command
    -column NAME does not exist in table/view DEPT

    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -607
    -Invalid command
    -column NAME does not exist in table/view DEPT
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -607
    -Invalid command
    -column "NAME" does not exist in table/view "PUBLIC"."DEPT"

    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -607
    -Invalid command
    -column "NAME" does not exist in table/view "PUBLIC"."DEPT"

    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -607
    -Invalid command
    -column "NAME" does not exist in table/view "PUBLIC"."DEPT"
"""

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
