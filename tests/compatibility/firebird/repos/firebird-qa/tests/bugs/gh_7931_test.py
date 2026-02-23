#coding:utf-8

"""
ID:          issue-7931
ISSUE:       7931
TITLE:       Incorrect variable usage using UPDATE OR INSERT
DESCRIPTION:
NOTES:
    Confirmed bug on 6.0.0.180.
    Checked on 6.0.0.186 (intermediate build for commit 305c40a05b1d64c14dbf5f25f36c42c44c6392d9) - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    set term ^;
    create or alter procedure sp_test as begin end
    ^
    recreate table test (id integer not null primary key)
    ^
    create or alter procedure sp_test(a_id integer) returns (o_result integer) as
        declare var integer = 1;
    begin
        update or insert into test (id) values (1 + 1) matching (id);
        o_result = var;
        suspend;
    end
    ^
    select * from sp_test(null)
    ^
"""

act = isql_act('db', test_script, substitutions=[ ('[ \t]+', ' '), ])

expected_stdout = """
    O_RESULT 1
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
