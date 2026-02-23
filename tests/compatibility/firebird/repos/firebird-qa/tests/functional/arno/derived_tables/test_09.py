#coding:utf-8

"""
ID:          derived-table-09
TITLE:       Outer reference inside derived table to other relations in from clause is not allowed
DESCRIPTION:
FBTEST:      functional.arno.derived_tables.09
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
  dt.*
FROM
  Table_10 t10
FULL JOIN (SELECT * FROM Table_10 t2 WHERE t2.ID = t10.ID) dt ON (1 = 1);"""

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
