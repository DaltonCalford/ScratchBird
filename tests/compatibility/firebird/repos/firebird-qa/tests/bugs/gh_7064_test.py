#coding:utf-8

"""
ID:          issue-7064
ISSUE:       7064
TITLE:       Linear regression functions aren't implemented correctly
DESCRIPTION:
FBTEST:      bugs.gh_7064
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select regr_avgx(a, b)
    from (
      select 1, 1 from RDB$DATABASE union all
      select 2, 1 from RDB$DATABASE union all
      select 3, 2 from RDB$DATABASE union all
      select 4, 2 from RDB$DATABASE
    ) t (a, b);

"""

act = isql_act('db', test_script)

expected_stdout = """
    1.500000000000000
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
