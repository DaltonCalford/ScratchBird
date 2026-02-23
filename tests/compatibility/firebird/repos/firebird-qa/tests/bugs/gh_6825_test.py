#coding:utf-8

"""
ID:          issue-6825
ISSUE:       6825
TITLE:       Correct error message for DROP VIEW
DESCRIPTION:
FBTEST:      bugs.gh_6825
NOTES:
    [04.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v1 as select 1 x from rdb$database;
    create or alter user tmp$gh_6825 password '123' using plugin Srp;
    commit;
    connect '$(DSN)' user tmp$gh_6825 password '123';
    drop view v1;
    commit;
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$gh_6825 using plugin Srp;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('(-)?Effective user is.*', '')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -DROP VIEW V1 failed
    -no permission for DROP access to VIEW V1
    -Effective user is TMP$GH_6825
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -DROP VIEW "PUBLIC"."V1" failed
    -no permission for DROP access to VIEW "PUBLIC"."V1"
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
