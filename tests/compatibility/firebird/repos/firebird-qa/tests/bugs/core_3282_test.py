#coding:utf-8

"""
ID:          issue-3650
ISSUE:       3650
TITLE:       EXECUTE STATEMENT parses the SQL text using wrong charset
DESCRIPTION:
JIRA:        CORE-3282
FBTEST:      bugs.core_3282
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3282.fbk')

test_script = """
    execute procedure TESTSP; -- 2.5.0 only: get "Malformed string" when connect with cset=utf8, confirmed 26.02.2015
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
