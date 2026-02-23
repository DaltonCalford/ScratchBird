#coding:utf-8

"""
ID:          issue-4792
ISSUE:       4792
TITLE:       Message "Modifying function <F> which is currently in use" when running script with AUTODDL=OFF and <F> is called from INTERNAL function declared in other unit
DESCRIPTION:
JIRA:        CORE-4472
FBTEST:      bugs.core_4472
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
    set autoddl off;
    commit;
    set term ^;
    create or alter function fn_01() returns int
    as begin
       return 1;
    end
    ^

    create or alter procedure sp_01
    as
        declare function fn_internal_01 returns int as
        begin
          if ( fn_01() > 0 ) then return 1;
          else return 0;
        end
    begin
    end
    ^
    set term ;^
    commit;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    log_before = act.get_firebird_log()
    act.expected_stdout = ''
    act.isql(switches=['-q', '-m'], input=test_script)
    log_after = act.get_firebird_log()
    assert act.clean_stdout == act.clean_expected_stdout
    assert list(unified_diff(log_before, log_after)) == []
