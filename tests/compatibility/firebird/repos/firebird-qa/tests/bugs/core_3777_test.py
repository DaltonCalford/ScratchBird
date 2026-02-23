#coding:utf-8

"""
ID:          issue-4121
ISSUE:       4121
TITLE:       Conversion error from string when using GROUP BY
DESCRIPTION:
JIRA:        CORE-3777
FBTEST:      bugs.core_3777
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TABLE2 (
    ID INTEGER NOT NULL,
    NAME VARCHAR(50)
);
ALTER TABLE TABLE2 ADD CONSTRAINT PK_TABLE2 PRIMARY KEY (ID);
COMMIT;
CREATE TABLE TABLE1 (
    ID INTEGER NOT NULL,
    ID_NAME INTEGER,
    NAME COMPUTED BY (COALESCE((SELECT NAME FROM TABLE2 WHERE ID = ID_NAME) || '.', '')),
    OTHER_FIELD INTEGER
);
ALTER TABLE TABLE1 ADD CONSTRAINT PK_TABLE1 PRIMARY KEY (ID);
COMMIT;
INSERT INTO TABLE2 (ID, NAME) VALUES (1, 'qqq');
INSERT INTO TABLE2 (ID, NAME) VALUES (2, 'www');
INSERT INTO TABLE2 (ID, NAME) VALUES (3, 'eee');
COMMIT;
INSERT INTO TABLE1 (ID, ID_NAME, OTHER_FIELD) VALUES (1, 1, 100);
INSERT INTO TABLE1 (ID, ID_NAME, OTHER_FIELD) VALUES (2, 2, 101);
INSERT INTO TABLE1 (ID, ID_NAME, OTHER_FIELD) VALUES (3, 3, 102);
COMMIT;"""

db = db_factory(init=init_script)

act = isql_act('db', "SELECT T.NAME FROM TABLE1 T GROUP BY T.NAME,T.OTHER_FIELD;")

expected_stdout = """
NAME
===================================================
eee.
qqq.
www.
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

