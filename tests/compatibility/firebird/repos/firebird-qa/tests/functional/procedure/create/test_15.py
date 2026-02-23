#coding:utf-8

"""
ID:          procedure.create-09
ISSUE:       1049
TITLE:       COLLATE IN STORED PROCEDURE
DESCRIPTION:
FBTEST:      functional.procedure.create.15
JIRA:        CORE-684
NOTES:
    [30.09.2023] pzotov
    Test not needed, see test_02.py and test_03.py. Added mark.skip().
"""

import pytest
from firebird.qa import *

init_script = """SET TERM !!;
CREATE PROCEDURE NEW_PROCEDURE (NOM1 VARCHAR(20) CHARACTER SET ISO8859_1 COLLATE FR_FR)
 RETURNS (NOM3 VARCHAR(20) CHARACTER SET ISO8859_1 COLLATE ISO8859_1)
 AS
 DECLARE VARIABLE NOM2 VARCHAR(20) CHARACTER SET ISO8859_1 COLLATE FR_CA;
BEGIN
 NOM2=NOM1;
 NOM3=NOM2;
 SUSPEND;
END !!
SET TERM ;!!
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SHOW PROCEDURE NEW_PROCEDURE;
SELECT * FROM NEW_PROCEDURE('TEST');"""

act = isql_act('db', test_script)

expected_stdout = """Procedure text:
=============================================================================
 DECLARE VARIABLE NOM2 VARCHAR(20) CHARACTER SET ISO8859_1 COLLATE FR_CA;
BEGIN
 NOM2=NOM1;
 NOM3=NOM2;
 SUSPEND;
END
=============================================================================
Parameters:
NOM1                              INPUT VARCHAR(20) CHARACTER SET ISO8859_1 COLLATE FR_FR
NOM3                              OUTPUT VARCHAR(20) CHARACTER SET ISO8859_1

NOM3
====================
TEST
"""

@pytest.mark.skip("Covered by test_02 and test_03")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
