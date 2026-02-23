#coding:utf-8

"""
ID:          issue-539
ISSUE:       539
TITLE:       SELECT...HAVING...NOT IN crashes server
DESCRIPTION:
  Crashed on: WI-V3.0.0.32380, WI-T4.0.0.32399, found 16-mar-2016.
  Passed on:  WI-V3.0.0.32487, WI-T4.0.0.141 -- works fine.
JIRA:        CORE-211
FBTEST:      bugs.core_0211
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select r.rdb$relation_id, count(*)
    from rdb$database r
    group by r.rdb$relation_id
    having count(*) not in (select r2.rdb$relation_id from rdb$database r2);
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$RELATION_ID[ ]+\\d+', 'RDB$RELATION_ID')])

expected_stdout = """
    RDB$RELATION_ID                 134
    COUNT                           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

