#coding:utf-8

"""
ID:          issue-4105
ISSUE:       4105
TITLE:       Conversion error when using a blob as an argument for the EXCEPTION statement
DESCRIPTION:
JIRA:        CORE-3761
FBTEST:      bugs.core_3761
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate
    delimiters without any statements between them (two semicolon, two carets etc).

    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create exception check_exception 'check exception';
    commit;
    set term ^;
    execute block as
    begin
        exception check_exception cast ('word' as blob sub_type text);
    end^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('(-)?At block line(:)?\\s+\\d+.*', '')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -CHECK_EXCEPTION
    -word
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."CHECK_EXCEPTION"
    -word
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
