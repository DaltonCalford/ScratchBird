#coding:utf-8

"""
ID:          derived-table-13
TITLE:       DISTINCT inside derived table
DESCRIPTION:
FBTEST:      functional.arno.derived_tables.13
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
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (4, 'five');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (5, 'five');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (6, 'five');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (7, 'seven');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (8, 'eight');
INSERT INTO Table_10 (ID, DESCRIPTION) VALUES (9, 'nine');

COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT
  dt.DESCRIPTION
FROM
(SELECT DISTINCT DESCRIPTION FROM Table_10 t10) dt (DESCRIPTION);"""

act = isql_act('db', test_script)

expected_stdout = """DESCRIPTION
===========
<null>
eight
five
nine
one
seven
three
two"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
