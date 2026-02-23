#coding:utf-8

"""
ID:          issue-6016
ISSUE:       6016
TITLE:       Parser should not allow to use GRANT OPTION for FUNCTION and PACKAGE
DESCRIPTION:
JIRA:        CORE-5753
FBTEST:      bugs.core_5753
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    set term ^;
    create or alter procedure sp_test as
    begin
    end
    ^
    create or alter function sa_func(a int) returns bigint as
    begin
      return a * a;
    end
    ^
    recreate package pg_test as
    begin
        function pg_func(a int) returns bigint;
    end
    ^
    create package body pg_test as
    begin
        function pg_func(a int) returns bigint as
        begin
            return a * a;
        end
    end
    ^
    set term ;^
    commit;

    -- following two statements have to raise error (but did not before fix):
    grant execute on procedure sp_test to function sa_func with grant option;
    grant execute on procedure sp_test to package pg_test with grant option;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Dynamic SQL Error
    -Using GRANT OPTION on functions not allowed

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -Dynamic SQL Error
    -Using GRANT OPTION on packages not allowed
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
