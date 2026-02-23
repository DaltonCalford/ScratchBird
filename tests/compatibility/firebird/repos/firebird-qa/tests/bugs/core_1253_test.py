#coding:utf-8

"""
ID:          issue-1677
ISSUE:       1677
TITLE:       LIST(DISTINCT) concatenate VARCHAR values as CHAR
DESCRIPTION:
JIRA:        CORE-1253
FBTEST:      bugs.core_1253
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T1 (C1 varchar(5));
COMMIT;
INSERT INTO T1 VALUES ('1');
INSERT INTO T1 VALUES ('2');
COMMIT;"""

db = db_factory(init=init_script)

test_script = """select list(distinct c1) from t1;
"""

act = isql_act('db', test_script)

expected_stdout = """LIST
=================
              0:1
==============================================================================
LIST:
1,2
==============================================================================

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

