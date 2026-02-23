#coding:utf-8

"""
ID:          issue-6018
ISSUE:       6018
TITLE:       No error if the GRANT target object does not exist
DESCRIPTION:
    grant execute on proc|func|package and grant usage on sequence|exception -- still does NOT produce error/warning.
    These statements temply disabled until some additional comments in tracker.
JIRA:        CORE-5755
FBTEST:      bugs.core_5755
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table table_test(x int);
    create or alter procedure sp_test as begin end;

    set term ^;
    create or alter function fn_test returns int as
    begin
        return cast( rand()*10000 as int );
    end
    ^

    create or alter package pkg_test as
    begin
        procedure sp_foo;
    end
    ^

    recreate package body pkg_test as
    begin
        procedure sp_foo as
            declare c int;
        begin
            c = 1;
        end
    end
    ^
    set term ;^

    recreate sequence g_test;
    recreate exception x_test 'foo!';
    commit;

    grant create table to function wrong_func;

    grant select on table_test to function wrong_func;


    grant execute on procedure sp_test to wrong_func;

    grant execute on function fn_test to wrong_func;

    grant execute on package pkg_test to wrong_func;

    grant usage on sequence g_test to wrong_func;

    grant usage on exception x_test to wrong_func;

"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Function WRONG_FUNC does not exist

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Function WRONG_FUNC does not exist
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Function "PUBLIC"."WRONG_FUNC" does not exist
    
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Function "PUBLIC"."WRONG_FUNC" does not exist
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
