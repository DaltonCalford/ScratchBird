#coding:utf-8

"""
ID:          issue-4398
ISSUE:       4398
TITLE:       NOT-NULL column can be used as primary key and filled with NULL-values
DESCRIPTION:
JIRA:        CORE-4070
FBTEST:      bugs.core_4070
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test01(uid char(16) character set octets collate octets);
    alter table test01 add constraint test01_pk primary key (uid);
"""

act = isql_act('db', test_script)

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE TEST01 failed
    -Column: UID not defined as NOT NULL - cannot be used in PRIMARY KEY constraint definition
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST01" failed
    -Column: "PUBLIC"."UID" not defined as NOT NULL - cannot be used in PRIMARY KEY constraint definition
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
