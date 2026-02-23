#coding:utf-8

"""
ID:          issue-2497
ISSUE:       2497
TITLE:       ALTER VIEW WITH CHECK OPTION crashes the engin
DESCRIPTION:
JIRA:        CORE-2061
FBTEST:      bugs.core_2061
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """create or alter view v as select * from rdb$database where 1 = 0 with check option;
alter view v as select * from rdb$database where 1 = 1 with check option;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
