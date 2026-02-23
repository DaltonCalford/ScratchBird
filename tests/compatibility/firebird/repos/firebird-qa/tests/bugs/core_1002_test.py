#coding:utf-8

"""
ID:          issue-1413
ISSUE:       1413
TITLE:       Bad handling of /*/ comments in ISQL
DESCRIPTION:
  Original title is: "set echo on" didn't work after /*////////////*/ comments in input file
JIRA:        CORE-1002
FBTEST:      bugs.core_1002
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """/*/ select * from rdb$database; /*/
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=2.1')
def test_1(act: Action):
    act.execute()
