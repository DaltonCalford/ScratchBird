#coding:utf-8

"""
ID:          issue-4079
ISSUE:       4079
TITLE:       GBAK fails to fix system generators while restoring
DESCRIPTION:
JIRA:        CORE-3733
FBTEST:      bugs.core_3733
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3733.fbk')

act_1 = isql_act('db', '')

@pytest.mark.version('>=3')
def test_1(act_1: Action):
    # Passes if test database is restored just fine
    pass

