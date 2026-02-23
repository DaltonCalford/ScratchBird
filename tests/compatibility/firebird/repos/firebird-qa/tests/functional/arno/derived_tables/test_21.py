#coding:utf-8

"""
ID:          derived-table-21
TITLE:       Implicit derived table by IN predicate
DESCRIPTION:
  IN predicate uses derived table internally and should ignore column-name checks
  (Aggregate functions are unnamed by default).
FBTEST:      functional.arno.derived_tables.21
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE Table_10 (
  ID INTEGER NOT NULL,
  DESCRIPTION VARCHAR(10)
);

COMMIT;

INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (0, NULL);
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (1, 'one');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (2, 'two');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (3, 'three');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (4, 'four');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (5, 'five');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (6, 'six');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (7, 'seven');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (8, 'eight');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (9, 'nine');

COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT
  t10.ID,
  t10.Description
FROM
  Table_10 t10
WHERE
t10.ID IN (SELECT MAX(t1.ID) FROM Table_10 t1);"""

act = isql_act('db', test_script)

expected_stdout = """          ID DESCRIPTION
============ ===========
9 nine"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
