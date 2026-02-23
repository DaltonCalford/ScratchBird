#coding:utf-8

"""
ID:          issue-4887
ISSUE:       4887
TITLE:       Wrong error at ALTER PACKAGE
DESCRIPTION:
JIRA:        CORE-4570
FBTEST:      bugs.core_4570
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create package p1
    as
    begin
      function f(x int) returns int;
    end
    ^
    create package body p1
    as
    begin
      function f1(x int) returns int;

      function f(x int) returns int
      as
      begin
        return f1(x) * x;
      end

      function f1(x int) returns int
      as
      begin
         return 1;
      end
    end
    ^
    commit
    ^

    alter package p1
    as
    begin
      function f(x int) returns int;
      function g(x int) returns int;
    end
    ^
    commit
    ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
