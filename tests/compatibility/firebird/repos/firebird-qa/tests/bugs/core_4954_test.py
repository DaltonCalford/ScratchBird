#coding:utf-8

"""
ID:          issue-5245
ISSUE:       5245
TITLE:       The package procedure with value by default isn't called if this parameter isn't specified
DESCRIPTION:
JIRA:        CORE-4954
FBTEST:      bugs.core_4954
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter package pkg_test
    as
    begin
      procedure p1(a int, b int = 1) returns(x int);
      function f1(a int,  b int = 1) returns int;
      procedure sp_test returns(x int);
    end
    ^

    recreate package body pkg_test
    as
    begin
      procedure p1(a int, b int) returns(x int) as
      begin
          x = a + b;
          suspend;
      end

      function f1(a int, b int) returns int as
      begin
          return a + b;
      end

      procedure sp_test returns(x int) as
      begin

        execute procedure p1( 12 ) returning_values :x; suspend;

        execute procedure p1( 12, 13 ) returning_values :x; suspend;

        select x from p1( 12 ) into x; suspend;
        select x from p1( 12, 13 ) into x; suspend;

        x = f1( 21 ); suspend;
        x = f1( 22, 23 ); suspend;

      end
    end
    ^
    set term ;^
    commit;


    set list on;
    select * from pkg_test.sp_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               13
    X                               25
    X                               13
    X                               25
    X                               22
    X                               45
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

