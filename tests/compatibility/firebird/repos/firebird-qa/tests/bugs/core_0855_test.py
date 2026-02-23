#coding:utf-8

"""
ID:          issue-1245
ISSUE:       1245
TITLE:       Aggregates in the WHERE clause vs derived tables
DESCRIPTION:
JIRA:        CORE-855
FBTEST:      bugs.core_0855
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select * from (
  select rdb$relation_id
  from rdb$database
)
where sum(rdb$relation_id) = 0;
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -104
-Cannot use an aggregate or window function in a WHERE clause, use HAVING (for aggregate only) instead
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

