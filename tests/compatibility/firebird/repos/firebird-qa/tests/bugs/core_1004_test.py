#coding:utf-8

"""
ID:          issue-1415
ISSUE:       1415
TITLE:       Context already in use (BLR error)
DESCRIPTION:
JIRA:        CORE-1004
FBTEST:      bugs.core_1004
"""

import pytest
from firebird.qa import *

init_script = """SET TERM ^;

CREATE OR ALTER PROCEDURE GET_REL_NAME (REL_ID INT) RETURNS (REL_NAME VARCHAR(32))
AS
BEGIN
  FOR SELECT R.RDB$RELATION_NAME
        FROM RDB$RELATIONS R
       WHERE R.RDB$RELATION_ID = :REL_ID
        INTO :REL_NAME
  DO
    SUSPEND;
END^

SET TERM ;^

COMMIT;

"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;

CREATE OR ALTER PROCEDURE BUG
AS
DECLARE C CURSOR FOR (
      SELECT (SELECT REL_NAME FROM GET_REL_NAME(R.RDB$RELATION_ID))
        FROM RDB$RELATIONS R
    );
DECLARE REL_NAME VARCHAR(32);
BEGIN
  OPEN C;
  WHILE (1 = 1) DO
  BEGIN
    FETCH C INTO :REL_NAME;

    IF (ROW_COUNT = 0)
    THEN LEAVE;
  END

  CLOSE C;
END^

SET TERM ;^

COMMIT;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
