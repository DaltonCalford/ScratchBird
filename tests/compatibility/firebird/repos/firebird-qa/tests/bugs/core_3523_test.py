#coding:utf-8

"""
ID:          issue-3880
ISSUE:       3880
TITLE:       SIMILAR TO: False matches on descending ranges
DESCRIPTION:
JIRA:        CORE-3523
FBTEST:      bugs.core_3523
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select 1 from rdb$database where 'm' similar to '[p-k]'
union
select 2 from rdb$database where 'z' similar to '[p-k]'
union
select 3 from rdb$database where 'm' not similar to '[p-k]'
union
select 4 from rdb$database where 'z' not similar to '[p-k]';

"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           3
           4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

