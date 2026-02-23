#coding:utf-8

"""
ID:          issue-2755
ISSUE:       2755
TITLE:       ALTER DOMAIN invalid RDB$FIELD_SUB_TYPE
DESCRIPTION:
JIRA:        CORE-2331
FBTEST:      bugs.core_2331
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN TESTDOM VARCHAR(50);
COMMIT;
ALTER DOMAIN TESTDOM TYPE VARCHAR(80);
COMMIT;

SELECT RDB$FIELD_SUB_TYPE FROM RDB$FIELDS WHERE RDB$FIELD_NAME = 'TESTDOM';
"""

act = isql_act('db', test_script)

expected_stdout = """
RDB$FIELD_SUB_TYPE
==================
                 0

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
