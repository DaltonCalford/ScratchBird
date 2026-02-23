#coding:utf-8

"""
ID:          issue-1420
ISSUE:       1420
TITLE:       Local buffer overrun in DYN_error() that takes down the server
DESCRIPTION:
    We have a local buffer overrun in DYN_error(), while copying tdbb_status_vector to
    local_status. It seems to be the first time (DYN errors + stack trace facility) when 20
    status words are not enough to store the complete error info.
JIRA:        CORE-1010
FBTEST:      bugs.core_1010
NOTES:
    [25.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Minimal snapshot number for 6.x: 6.0.0.863, see letter from Adriano, 24.06.2025 13:12, commit:
    https://github.com/FirebirdSQL/firebird/commit/9c6855b516de4e4aea78e7df782e297f4e220287

    Checked on 6.0.0.863; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    create exception ex_test 'Trigger suddenly was created!';
    commit;

    set term ^ ;
    create or alter trigger rdb$procedures_biu for rdb$procedures
    active after update or delete position 0 as
    begin
        exception ex_test;
    end
    ^
    commit^

    create or alter procedure proctest returns (result integer) as
    begin
        result = 0;
        suspend;
    end^
    set term ; ^
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE OR ALTER TRIGGER RDB$PROCEDURES_BIU failed
    -no permission for ALTER access to TABLE RDB$PROCEDURES
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 28000
    CREATE OR ALTER TRIGGER "SYSTEM"."RDB$PROCEDURES_BIU" failed
    -Cannot CREATE/ALTER/DROP TRIGGER in SYSTEM schema
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

