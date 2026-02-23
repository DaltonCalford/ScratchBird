#coding:utf-8

"""
ID:          issue-2681
ISSUE:       2681
TITLE:       '...exception...string right truncation' when alter view with join
DESCRIPTION:
JIRA:        CORE-2255
FBTEST:      bugs.core_2255
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST_VARCHAR (
    ID INTEGER,
    CAPTION VARCHAR(1024)
);

INSERT INTO TEST_VARCHAR (ID, CAPTION) VALUES (1, 'CAP_1');

CREATE VIEW P_TEST_VARCHAR(
    ID,
    CAPTION)
AS
SELECT
    T1.ID,
    T1.CAPTION
FROM TEST_VARCHAR T1;
"""

db = db_factory(init=init_script)

test_script = """ALTER VIEW P_TEST_VARCHAR(
    ID)
AS
SELECT
    T1.ID
FROM TEST_VARCHAR T1, TEST_VARCHAR T2
  WHERE T1.ID = T2.ID;

SELECT * FROM P_TEST_VARCHAR;
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID
============
           1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

