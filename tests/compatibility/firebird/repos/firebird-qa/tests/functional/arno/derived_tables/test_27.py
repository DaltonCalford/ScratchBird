#coding:utf-8

"""
ID:          derived-table-27
TITLE:       Derived table with same alias for several columns
DESCRIPTION: Expect an error if a derived table column aliases is not unique.
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
  (SELECT * FROM Table_10 t10) dt (ID, ID);"""

act = isql_act('db', test_script)

expected_stderr = """
Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -104
-Invalid command
-column ID was specified multiple times for derived table DT
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr