#coding:utf-8

"""
ID:          issue-4401
ISSUE:       4401
TITLE:       Constant columns getting empty value with subselect from procedure
DESCRIPTION:
JIRA:        CORE-4073
FBTEST:      bugs.core_4073
"""

import pytest
from firebird.qa import *

init_script = """
    create domain d_vc10 varchar(10);
    commit;
    set term ^;
    create or alter procedure P_TEST returns (TEXT D_VC10) as
    begin
      TEXT = '12345'; suspend;
    end^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select A, TEXT from (select '2' as A, TEXT from P_TEST);
"""

act = isql_act('db', test_script)

expected_stdout = """
    A                               2
    TEXT                            12345
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

