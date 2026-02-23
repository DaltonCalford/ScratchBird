#coding:utf-8

"""
ID:          issue-1407
ISSUE:       1407
TITLE:       Keyword AS not recognized in clause FROM
DESCRIPTION: The sentence SELECT * FROM <table> AS <alias> is not recognized correct.
JIRA:        CORE-996
FBTEST:      bugs.core_0996
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T1 (ID1 INTEGER NOT NULL);
INSERT INTO T1 VALUES (1);
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT ID1 from T1 AS BLA;
"""

act = isql_act('db', test_script)

expected_stdout = """         ID1
============
           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

