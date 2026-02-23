#coding:utf-8

"""
ID:          issue-1651
ISSUE:       1651
TITLE:       LIST() function seems not work if used twice or more in a query
DESCRIPTION:
  If I try to use the LIST() function twice or more in a query the following error occurs:
    Undefined name.
    Dynamic SQL Error.
    SQL error code = -204.
    Implementation limit exceeded.
    Block size exceeds implementation restriction.
JIRA:        CORE-1227
FBTEST:      bugs.core_1227
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TESTTABLE (ID integer, FIELD1 char(5), FIELD2 char(20));
INSERT INTO TESTTABLE VALUES (1,'aaaaa','bbbbbbbbb');
INSERT INTO TESTTABLE VALUES (1,'ccccc','ddddddddd');
"""

db = db_factory(init=init_script)

test_script = """SELECT LIST(FIELD1), LIST(FIELD2) FROM TESTTABLE GROUP BY ID;
"""

act = isql_act('db', test_script)

expected_stdout = """             LIST              LIST
================= =================
              0:1               0:2
==============================================================================
LIST:
aaaaa,ccccc
==============================================================================
==============================================================================
LIST:
bbbbbbbbb           ,ddddddddd
==============================================================================

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

