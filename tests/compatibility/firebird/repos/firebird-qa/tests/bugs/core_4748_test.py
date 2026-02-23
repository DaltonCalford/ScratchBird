#coding:utf-8

"""
ID:          issue-5053
ISSUE:       5053
TITLE:       Can not restore in FB 3.0 SuperServer from .fbk which source .fdb was created
  on 2.5.4 and moved to READ-ONLY before backed up
DESCRIPTION:
JIRA:        CORE-4748
FBTEST:      bugs.core_4748
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core_4748_read_only_25.fbk', async_write=False)

test_script = """
    set list on;
    select mon$read_only as restored_ro from mon$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESTORED_RO                     1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

