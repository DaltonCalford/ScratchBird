#coding:utf-8

"""
ID:          issue-2732
ISSUE:       2732
TITLE:       SIMILAR TO produces random results with [x-y] expressions
DESCRIPTION:
JIRA:        CORE-2308
FBTEST:      bugs.core_2308
"""

import pytest
from firebird.qa import *

init_script = """set term ^ ;
CREATE OR ALTER PROCEDURE PROC
RETURNS ( V INTEGER)
AS
 BEGIN
  IF ('b' SIMILAR TO ('[a-z]'))
  THEN v = 1;
  ELSE v = 2;
  SUSPEND;
END ^
"""

db = db_factory(init=init_script)

test_script = """set term ^ ;
EXECUTE BLOCK AS
DECLARE I INT = 1000;
DECLARE V INT;
BEGIN
  WHILE (I > 0) DO
  BEGIN
    I = I - 1;
    SELECT V FROM PROC INTO :V;

    IF (V <> 1)
    THEN V = 1/0;
  END
END ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
