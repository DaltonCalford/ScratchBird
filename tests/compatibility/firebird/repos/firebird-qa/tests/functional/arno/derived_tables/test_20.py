#coding:utf-8

"""
ID:          derived-table-20
TITLE:       Sub-select inside derived table
DESCRIPTION:
FBTEST:      functional.arno.derived_tables.20
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE Table_10 (
  ID INTEGER NOT NULL,
  GROUPID INTEGER,
  DESCRIPTION VARCHAR(10)
);

COMMIT;

INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (0, NULL, NULL);
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (1, 1, 'one');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (2, 1, 'two');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (3, 2, 'three');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (4, 2, 'four');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (5, 2, 'five');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (6, 3, 'six');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (7, 3, 'seven');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (8, 3, 'eight');
INSERT INTO Table_10 (ID, GROUPID, DESCRIPTION) VALUES (9, 3, 'nine');

COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT
  dt.*
FROM
  (SELECT
    t2.GROUPID,
    (SELECT MIN(t1.ID) FROM Table_10 t1 WHERE t1.GROUPID = t2.GROUPID),
    (SELECT MAX(t1.ID) FROM Table_10 t1 WHERE t1.GROUPID = t2.GROUPID)
   FROM
     Table_10 t2
   GROUP BY
t2.GROUPID) dt (GROUPID, MIN_ID, MAX_ID);"""

act = isql_act('db', test_script)

expected_stdout = """     GROUPID       MIN_ID       MAX_ID
============ ============ ============
      <null>       <null>       <null>
           1            1            2
           2            3            5
3            6            9"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
