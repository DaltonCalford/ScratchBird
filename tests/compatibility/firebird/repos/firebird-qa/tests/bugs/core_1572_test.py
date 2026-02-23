#coding:utf-8

"""
ID:          issue-1990
ISSUE:       1990
TITLE:       Multiple Rows in Singleton Select not reported in a View
DESCRIPTION:
JIRA:        CORE-1572
FBTEST:      bugs.core_1572
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TABLE1 ( ID1 INTEGER NOT NULL, FIELD1 VARCHAR(20));
CREATE TABLE TABLE2 ( ID2 INTEGER NOT NULL, FIELD2 VARCHAR(20));

INSERT INTO TABLE1 (ID1, FIELD1) VALUES (1, 'ONE');
INSERT INTO TABLE2 (ID2, FIELD2) VALUES (2, 'TWO');
INSERT INTO TABLE2 (ID2, FIELD2) VALUES (3, 'THREE');

CREATE VIEW VIEW_TABLE( ID1, FIELD1, FIELD2) AS
SELECT TABLE1.ID1, TABLE1.FIELD1, ( SELECT TABLE2.FIELD2 FROM TABLE2 ) FROM TABLE1;
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT * FROM VIEW_TABLE;
"""

act = isql_act('db', test_script)

expected_stdout = """
         ID1 FIELD1               FIELD2
============ ==================== ====================
"""

expected_stderr = """Statement failed, SQLSTATE = 21000
multiple rows in singleton select
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

