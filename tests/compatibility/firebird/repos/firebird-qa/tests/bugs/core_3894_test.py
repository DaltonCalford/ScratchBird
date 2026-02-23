#coding:utf-8

"""
ID:          issue-4230
ISSUE:       4230
TITLE:       Wrong numbers in error message for decreasing char/varchar columns
DESCRIPTION:
JIRA:        CORE-3894
FBTEST:      bugs.core_3894
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
    set autoddl off;
    recreate table test(id int);
    commit;

    alter table test add s01 varchar(8188) character set utf8;
    commit;

    alter table test alter column s01 type varchar(8189) character set utf8;
    alter table test alter column s01 type varchar(8190) character set utf8;
    alter table test alter column s01 type varchar(8189) character set utf8;
    commit;

    show table test;
"""

substitutions=[('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE TEST failed
    -New size specified for column S01 must be at least 8190 characters.
    ID                              INTEGER Nullable
    S01                             VARCHAR(8190) CHARACTER SET UTF8 Nullable
"""
expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST" failed
    -New size specified for column "S01" must be at least 8190 characters.
    Table: PUBLIC.TEST
    ID                              INTEGER Nullable
    S01                             VARCHAR(8190) CHARACTER SET SYSTEM.UTF8 Nullable
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
