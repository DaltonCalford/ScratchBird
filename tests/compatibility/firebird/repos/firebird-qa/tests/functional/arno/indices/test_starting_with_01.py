#coding:utf-8

"""
ID:          index.starting-with-01
TITLE:       STARTING WITH charset NONE
DESCRIPTION: STARTING WITH - Select from table with 2 entries
FBTEST:      functional.arno.indices.starting_with_01
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE A_TEST (F1 VARCHAR(5), F2 VARCHAR(5), F3 VARCHAR(5));
CREATE INDEX IDX_A_TEST_F1 ON A_TEST(F1);
CREATE DESC INDEX IDX_A_TEST_F2 ON A_TEST(F2);
COMMIT;
INSERT INTO A_TEST (F1, F2, F3) VALUES('', '', '');
INSERT INTO A_TEST (F1, F2, F3) VALUES(NULL, NULL, NULL);
INSERT INTO A_TEST (F1, F2, F3) VALUES('a', 'a', 'a');
INSERT INTO A_TEST (F1, F2, F3) VALUES('b', 'b', 'b');
COMMIT;

SET TERM ^;

CREATE PROCEDURE PR_A_TEST_STARTING_WITH(I_START_VALUE VARCHAR(5))
RETURNS(O_FIELD VARCHAR(32), O_COUNT integer)
AS
BEGIN
  /* First retrieve results for indexed ASC field */
  O_FIELD = 'F1 - INDEXED ASC';
  SELECT COUNT(*) FROM A_TEST
  WHERE F1 STARTING WITH :I_START_VALUE
  INTO :O_COUNT;

  SUSPEND;

  /* Second retrieve results for indexed DESC field */
  O_FIELD = 'F2 - INDEXED DESC';
  SELECT COUNT(*) FROM A_TEST
  WHERE F2 STARTING WITH :I_START_VALUE
  INTO :O_COUNT;

  SUSPEND;

  /* Thirth for unindexed field */
  O_FIELD = 'F3 - NOT INDEXED';
  SELECT COUNT(*) FROM A_TEST
  WHERE F3 STARTING WITH :I_START_VALUE
  INTO :O_COUNT;

  SUSPEND;
END^

SET TERM ;^

"""

db = db_factory(init=init_script)

test_script = """SET PLAN OFF;
SELECT O_FIELD, O_COUNT FROM PR_A_TEST_STARTING_WITH('');"""

act = isql_act('db', test_script)

expected_stdout = """O_FIELD                               O_COUNT
================================ ============

F1 - INDEXED ASC                            3
F2 - INDEXED DESC                           3
F3 - NOT INDEXED                            3"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
