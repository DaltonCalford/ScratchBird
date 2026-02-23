#coding:utf-8

"""
ID:          issue-5687
ISSUE:       5687
TITLE:       Error restoring on FB 3.0 from FB 2.5: bugcheck 221 (cannot remap)
DESCRIPTION:
  Test verifies only issue 08/Dec/16 02:47 PM (pointed out by ASF).
JIRA:        CORE-5414
FBTEST:      bugs.core_5414
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 1 x from rdb$database a full join rdb$database b on (exists(select 1 from rdb$database));
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               1
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

