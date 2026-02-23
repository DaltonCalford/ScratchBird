#coding:utf-8

"""
ID:          issue-2257
ISSUE:       2257
TITLE:       Error with ABS in dialect 1
DESCRIPTION:
JIRA:        CORE-1828
FBTEST:      bugs.core_1828
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST (MYNUM INTEGER);
COMMIT;
INSERT INTO TEST (MYNUM) VALUES (1);
INSERT INTO TEST (MYNUM) VALUES (-1);
INSERT INTO TEST (MYNUM) VALUES (2147483647);
INSERT INTO TEST (MYNUM) VALUES (-2147483648);
COMMIT;
"""

db = db_factory(charset='UTF8', sql_dialect=1, init=init_script)

act = isql_act('db', "SELECT ABS(MYNUM) FROM TEST;")

expected_stdout = """
                    ABS
=======================
      1.000000000000000
      1.000000000000000
      2147483647.000000
      2147483648.000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

