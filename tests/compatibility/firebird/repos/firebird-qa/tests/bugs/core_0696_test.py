#coding:utf-8

"""
ID:          issue-1065
ISSUE:       1065
TITLE:       User Account maintanance in SQL
DESCRIPTION:
JIRA:        CORE-696
FBTEST:      bugs.core_0696
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE USER alex PASSWORD 'test';
COMMIT;
ALTER USER alex FIRSTNAME 'Alex' LASTNAME 'Peshkov';
COMMIT;
ALTER USER alex PASSWORD 'IdQfA';
COMMIT;
DROP USER alex;
COMMIT;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
