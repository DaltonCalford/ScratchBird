#coding:utf-8

"""
ID:          issue-1263
ISSUE:       1263
TITLE:       Incorrect handling of null within view - returns 0
DESCRIPTION:
JIRA:        CORE-336
FBTEST:      bugs.core_0871
"""

import pytest
from firebird.qa import *

init_script = """CREATE DOMAIN D INTEGER NOT NULL;
CREATE TABLE T (A D);
CREATE TABLE U (B D);
CREATE VIEW V (A, B) AS
SELECT T.A, U.B FROM T LEFT JOIN U ON (T.A = U.B);

COMMIT;

INSERT INTO T VALUES(1);
COMMIT;

"""

db = db_factory(init=init_script)

test_script = """SELECT * FROM V;
"""

act = isql_act('db', test_script)

expected_stdout = """A            B
============ ============
           1       <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

