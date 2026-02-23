#coding:utf-8

"""
ID:          issue-5699
ISSUE:       5699
TITLE:       Error on field concatenation of System tables
DESCRIPTION:
JIRA:        CORE-5427
FBTEST:      bugs.core_5427
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 0*char_length(txt) x1, 0*octet_length(txt) x2
    from (
      select 'generator '|| r.rdb$generator_name ||' .' as txt from rdb$generators r
      order by 1 rows 1
    )
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X1                              0
    X2                              0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

