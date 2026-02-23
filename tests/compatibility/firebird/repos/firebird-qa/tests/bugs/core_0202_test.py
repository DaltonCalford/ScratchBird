#coding:utf-8

"""
ID:          issue-529
ISSUE:       529
TITLE:       ORDER BY works wrong with collate PT_PT
DESCRIPTION:
JIRA:        CORE-202
FBTEST:      bugs.core_202
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE STOCKS (
    MNEN    INTEGER NOT NULL,
    ACTIVO  VARCHAR(50) CHARACTER SET ISO8859_1 COLLATE PT_PT
);

COMMIT WORK;

INSERT INTO STOCKS (MNEN, ACTIVO) VALUES (1, 'B&A');
INSERT INTO STOCKS (MNEN, ACTIVO) VALUES (2, 'BES');
INSERT INTO STOCKS (MNEN, ACTIVO) VALUES (3, 'BCP');
INSERT INTO STOCKS (MNEN, ACTIVO) VALUES (4, 'B&A Pref.');
INSERT INTO STOCKS (MNEN, ACTIVO) VALUES (5, 'Banif');

COMMIT WORK;

CREATE COLLATION PT_PT2 FOR ISO8859_1 FROM PT_PT 'SPECIALS-FIRST=1';

COMMIT WORK;

CREATE TABLE STOCKS2 (
    MNEN    INTEGER NOT NULL,
    ACTIVO  VARCHAR(50) CHARACTER SET ISO8859_1 COLLATE PT_PT2
);

COMMIT WORK;

INSERT INTO STOCKS2 (MNEN, ACTIVO) VALUES (1, 'B&A');
INSERT INTO STOCKS2 (MNEN, ACTIVO) VALUES (2, 'BES');
INSERT INTO STOCKS2 (MNEN, ACTIVO) VALUES (3, 'BCP');
INSERT INTO STOCKS2 (MNEN, ACTIVO) VALUES (4, 'B&A Pref.');
INSERT INTO STOCKS2 (MNEN, ACTIVO) VALUES (5, 'Banif');

COMMIT WORK;

"""

db = db_factory(init=init_script)

test_script = """SELECT ACTIVO FROM STOCKS ORDER BY ACTIVO;

SELECT ACTIVO FROM STOCKS2 ORDER BY ACTIVO;
"""

act = isql_act('db', test_script)

expected_stdout = """ACTIVO
==================================================
B&A
Banif
B&A Pref.
BCP
BES

ACTIVO
==================================================
B&A
B&A Pref.
Banif
BCP
BES

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

