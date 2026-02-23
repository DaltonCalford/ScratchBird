#coding:utf-8

"""
ID:          issue-1854
ISSUE:       1854
TITLE:       Outer joins don't work properly with the MON$ tables
DESCRIPTION:
JIRA:        CORE-1436
FBTEST:      bugs.core_1436
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select db.MON$FORCED_WRITES fw
    from mon$attachments att
    left join mon$database db on db.mon$creation_date = att.mon$timestamp
    rows 1;
    -- select db.mon$database_name
    --  from mon$attachments att
    --  left join mon$database db on db.mon$creation_date = att.mon$timestamp;
"""

act = isql_act('db', test_script)

expected_stdout = """
    FW                              <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

