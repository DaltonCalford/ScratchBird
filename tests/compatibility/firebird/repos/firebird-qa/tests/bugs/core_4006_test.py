#coding:utf-8

"""
ID:          issue-4338
ISSUE:       4338
TITLE:       using a result from a procedure in a substring expression leads to server crash
DESCRIPTION:
JIRA:        CORE-4006
FBTEST:      bugs.core_4006
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^ ;
    create or alter procedure p_str_rpos
    returns (
        result integer)
    as
    begin
       result=14;
      suspend;
    end^
    set term ; ^
    commit;

    set list on;
    select substring('somestringwith \\ no meaning' from 1 for result) r
    from p_str_rpos;
"""

act = isql_act('db', test_script)

expected_stdout = """
    R                               somestringwith
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

