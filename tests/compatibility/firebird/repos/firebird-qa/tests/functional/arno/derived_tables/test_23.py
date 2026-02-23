#coding:utf-8

"""
ID:          derived-table-23
TITLE:       Derived table with outer reference from JOIN.
DESCRIPTION: Outer reference inside derived table to other relations in from clause is not allowed.
NOTES:
    [09.03.2023] pzotov
    This test was missing as a result of migration for unknown reason.
    Thanks to Anton Zuev for note.
    Checked on 3.0.11.33665, 4.0.3.2904, 5.0.0.970
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
COMMIT;"""

db = db_factory(init=init_script)

test_script = """SELECT
  dt.*
FROM
  Table_10 t10
  JOIN (SELECT * FROM Table_10 t2 WHERE t2.ID = t10.ID) dt ON (1 = 1);"""

substitutions = [('^((?!(SQLSTATE|Column unknown)).)*$', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42S22
    -Column unknown
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
