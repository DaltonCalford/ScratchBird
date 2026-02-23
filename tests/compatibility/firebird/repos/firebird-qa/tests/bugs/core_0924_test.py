#coding:utf-8

"""
ID:          issue-1324
ISSUE:       1324
TITLE:       An attempt to select DB_KEY from a procedure crashes the server
DESCRIPTION:
JIRA:        CORE-924
FBTEST:      bugs.core_0924
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T1 (ID1 INTEGER NOT NULL);
SET TERM ^;
CREATE PROCEDURE AP
returns (output1 INTEGER)
AS
BEGIN
 FOR SELECT ID1 FROM T1 INTO :output1
 DO
  BEGIN
   SUSPEND;
  END
END ^
SET TERM ;^
COMMIT;
INSERT INTO T1 VALUES (1);
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT RDB$DB_KEY from AP;
"""

act = isql_act('db', test_script,
               substitutions=[('line\\s[0-9]+,', 'line x'), ('column\\s[0-9]+', 'column y')])

expected_stderr = """Statement failed, SQLSTATE = 42S22
Dynamic SQL Error
-SQL error code = -206
-Column unknown
-DB_KEY
-At line 1, column 8
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

