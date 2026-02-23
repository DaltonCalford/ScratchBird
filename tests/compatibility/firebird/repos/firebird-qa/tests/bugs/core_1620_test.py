#coding:utf-8

"""
ID:          issue-2041
ISSUE:       2041
TITLE:       Incorrect error message (column number) if the empty SQL string is prepared
DESCRIPTION:
JIRA:        CORE-1620
FBTEST:      bugs.core_1620
NOTES:
    [25.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create procedure test_es1 as
    begin
        execute statement '';
    end
    ^
    set term ;^
    commit;
    execute procedure test_es1;
"""

# ::: ACHTUNG :::
# DO NOT use any substitutions here!
# We have to check EXACTLY that error message contains
# proper (adequate) column value in:
# "-Unexpected end of command - line 1, column 1"
#
act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command - line 1, column 1
    -At procedure 'TEST_ES1' line: 3, col: 9
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command - line 1, column 1
    -At procedure "PUBLIC"."TEST_ES1" line: 3, col: 9
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
