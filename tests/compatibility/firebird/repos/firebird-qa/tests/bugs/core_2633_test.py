#coding:utf-8

"""
ID:          issue-3041
ISSUE:       3041
TITLE:       SELECT WITH LOCK with no fields are accessed clears the data
DESCRIPTION:
JIRA:        CORE-2633
FBTEST:      bugs.core_2633
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T (A VARCHAR(20), B INTEGER);
INSERT INTO T(A,B) VALUES('aaaa',1);
INSERT INTO T(A,B) VALUES('bbbb',2);
INSERT INTO T(A,B) VALUES('cccc',3);
COMMIT;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """SELECT * FROM T;

SET TERM ^;
EXECUTE BLOCK AS
DECLARE I INTEGER;
BEGIN
  FOR SELECT 1 FROM T WITH LOCK INTO :I DO I=I;
END^
SET TERM ;^

SELECT * FROM T;

ROLLBACK;

SELECT * FROM T;
"""

act = isql_act('db', test_script)

expected_stdout = """
A                               B
==================== ============
aaaa                            1
bbbb                            2
cccc                            3


A                               B
==================== ============
aaaa                            1
bbbb                            2
cccc                            3


A                               B
==================== ============
aaaa                            1
bbbb                            2
cccc                            3

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

