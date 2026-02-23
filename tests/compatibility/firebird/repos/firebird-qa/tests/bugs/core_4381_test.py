#coding:utf-8

"""
ID:          issue-4703
ISSUE:       4703
TITLE:       Incorrect line/column information in runtime errors
DESCRIPTION:
JIRA:        CORE-4381
FBTEST:      bugs.core_4381
NOTES:
    [29.06.2025] pzotov
    Added subst to suppress displaying name of stored procedure: on 6.x it is prefixed by SQL schema and enclosed in quotes.
    For this test it is enough just to show proper numbers of line and column.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
set term ^;



create or alter procedure p1 returns (x integer) as
begin
                           select
                                                 'a'
                           from rdb$database
                                             into x;
end
^

execute procedure p1
^
"""

substitutions = [('-At procedure \\S+', 'At procedure')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "a"
    At procedure line: 3, col: 28
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
