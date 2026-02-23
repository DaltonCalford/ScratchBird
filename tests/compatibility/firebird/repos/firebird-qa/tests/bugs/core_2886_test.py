#coding:utf-8

"""
ID:          issue-3270
ISSUE:       3270
TITLE:       Query with "NOT IN <subselect from view>" fails
DESCRIPTION:
JIRA:        CORE-2886
FBTEST:      bugs.core_2886
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T (ID INTEGER NOT NULL);

CREATE VIEW V( ID) AS select ID from T;

INSERT INTO T (ID) VALUES (1);
INSERT INTO T (ID) VALUES (2);
INSERT INTO T (ID) VALUES (3);

COMMIT;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """SELECT ID FROM T
WHERE ID NOT IN
  (SELECT ID FROM V WHERE ID = 1);
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID
============
           2
           3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

