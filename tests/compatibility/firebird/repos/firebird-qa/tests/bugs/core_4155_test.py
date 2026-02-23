#coding:utf-8

"""
ID:          issue-4482
ISSUE:       4482
TITLE:       External routines DDL in Packages wrongly report error for termination with semi-colon
DESCRIPTION:
JIRA:        CORE-4155
FBTEST:      bugs.core_4155
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter package pkg
    as
    begin
        function sum_args (
            n1 integer,
            n2 integer,
            n3 integer
        ) returns integer;
    end
    ^
    recreate package body pkg
    as
    begin
        function sum_args (
            n1 integer,
            n2 integer,
            n3 integer
        ) returns integer
            external name 'udrcpp_example!sum_args'
            engine udr; -- error with the semi-colon and works without it
    end
    ^
    set term ;^
    commit;
    set list on;
    select pkg.sum_args(111,555,777) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    SUM_ARGS                        1443
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

