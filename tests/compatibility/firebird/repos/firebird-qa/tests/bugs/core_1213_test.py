#coding:utf-8

"""
ID:          issue-1637
ISSUE:       1637
TITLE:       CURRENT OF support views
DESCRIPTION:
JIRA:        CORE-1213
FBTEST:      bugs.core_1213
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TAB1 (COL1 INTEGER);
CREATE VIEW V1 (COL1) AS SELECT COL1 FROM TAB1;
COMMIT;
INSERT INTO TAB1 (COL1) VALUES (1);
INSERT INTO TAB1 (COL1) VALUES (2);
INSERT INTO TAB1 (COL1) VALUES (3);
COMMIT;
SET TERM ^ ;
CREATE PROCEDURE P1 AS
DECLARE vCOL1 INTEGER;
BEGIN
FOR SELECT COL1 FROM V1 WHERE COL1 IN (2,3) INTO :vCOL1 AS CURSOR VIEW_CURSOR DO
  BEGIN
   DELETE FROM V1 WHERE CURRENT OF VIEW_CURSOR;
  END
END ^
COMMIT ^


"""

db = db_factory(init=init_script)

test_script = """SELECT COL1 FROM V1;
EXECUTE PROCEDURE P1;
SELECT COL1 FROM V1;
"""

act = isql_act('db', test_script)

expected_stdout = """
        COL1
============
           1
           2
           3


        COL1
============
           1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

