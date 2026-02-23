#coding:utf-8

"""
ID:          issue-4211
ISSUE:       4211
TITLE:       Computed column appears in non-existant rows of left join
DESCRIPTION:
JIRA:        CORE-3874
FBTEST:      bugs.core_3874
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST_TABLE (
  ID INTEGER,
  COMPUTED_COL VARCHAR(6) COMPUTED BY ('FAILED')
  );
"""

db = db_factory(init=init_script)

test_script = """SELECT t.COMPUTED_COL
FROM RDB$DATABASE r
LEFT JOIN TEST_TABLE t
ON r.RDB$RELATION_ID = t.ID;
"""

act = isql_act('db', test_script)

expected_stdout = """COMPUTED_COL
============
<null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

