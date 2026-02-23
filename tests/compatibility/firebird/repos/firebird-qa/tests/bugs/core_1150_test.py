#coding:utf-8

"""
ID:          issue-1572
ISSUE:       1572
TITLE:       Error conversion error from string " " using outer join on int64 and int fields
DESCRIPTION:
JIRA:        CORE-1150
FBTEST:      bugs.core_1150
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE J (
    ID INTEGER NOT NULL,
    CODETABLE INTEGER,
    CODEVSPTABLE INTEGER
);


CREATE TABLE TT (
    ID BIGINT NOT NULL
);

ALTER TABLE TT ADD CONSTRAINT PK_TT PRIMARY KEY (ID);

COMMIT;

INSERT INTO TT(ID) VALUES(1);
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SELECT 1
  FROM TT T1 LEFT JOIN J ON J.CODETABLE = T1.ID
       LEFT JOIN TT T2 ON J.CODEVSPTABLE = T2.ID ;
"""

act = isql_act('db', test_script)

expected_stdout = """CONSTANT
============
           1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

