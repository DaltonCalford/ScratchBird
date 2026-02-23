#coding:utf-8

"""
ID:          issue-4612
ISSUE:       4612
TITLE:       Regression: NOT-null field from derived table became NULL when is referred outside DT
DESCRIPTION:
JIRA:        CORE-4289
FBTEST:      bugs.core_4289
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
set list on;
select q.n, case when q.n=0 then 'zero' when q.n<>0 then 'NON-zero' else 'Hm!..' end what_is_n
from (select 0 N from RDB$DATABASE) q;
"""

act = isql_act('db', test_script)

expected_stdout = """
N                               0
WHAT_IS_N                       zero
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
