#coding:utf-8

"""
ID:          issue-2865
ISSUE:       2865
TITLE:       Query SELECT ... WHERE ... IN (SELECT DISTINCT ... ) returns a wrong result set
DESCRIPTION:
JIRA:        CORE-2451
FBTEST:      bugs.core_2451
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TBL_TEST (FLD_VALUE INTEGER);
INSERT INTO TBL_TEST VALUES (1);
INSERT INTO TBL_TEST VALUES (2);
INSERT INTO TBL_TEST VALUES (3);
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT * FROM TBL_TEST WHERE FLD_VALUE IN (SELECT DISTINCT FLD_VALUE FROM TBL_TEST WHERE FLD_VALUE NOT IN (SELECT DISTINCT FLD_VALUE FROM TBL_TEST));
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
