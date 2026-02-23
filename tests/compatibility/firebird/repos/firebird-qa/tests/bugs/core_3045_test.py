#coding:utf-8

"""
ID:          issue-3426
ISSUE:       3426
TITLE:       "conversion error from string" after change of field type from BIGINT to VARCHAR(21)
DESCRIPTION:
JIRA:        CORE-3045
FBTEST:      bugs.core_3045
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST1(
  ID INTEGER,
  TEST_FIELD BIGINT,

  PRIMARY KEY(ID)
);

COMMIT;

INSERT INTO TEST1(ID, TEST_FIELD)
VALUES(1, 234);

COMMIT;"""

db = db_factory(init=init_script)

test_script = """ALTER TABLE TEST1
  ALTER TEST_FIELD TYPE VARCHAR(21);

COMMIT;

SELECT ID, TEST_FIELD
FROM TEST1
WHERE TEST_FIELD = 'A';
SELECT ID, TEST_FIELD
FROM TEST1
WHERE TEST_FIELD != 'A';
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID TEST_FIELD
============ =====================
           1 234

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

