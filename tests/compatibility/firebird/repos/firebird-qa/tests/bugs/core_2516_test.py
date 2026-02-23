#coding:utf-8

"""
ID:          issue-2926
ISSUE:       2926
TITLE:       Wrong processing SP parameters with arrays
DESCRIPTION:
JIRA:        CORE-2516
FBTEST:      bugs.core_2516
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain t_smallint_array as smallint [0:2];
    set term ^;
    create procedure sp_smallint_array(x t_smallint_array) returns (y t_smallint_array) as
    begin
        y=x;
        suspend;
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 0A000
    CREATE PROCEDURE SP_SMALLINT_ARRAY failed
    -Dynamic SQL Error
    -feature is not supported
    -Usage of domain or TYPE OF COLUMN of array type in PSQL
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 0A000
    CREATE PROCEDURE "PUBLIC"."SP_SMALLINT_ARRAY" failed
    -Dynamic SQL Error
    -feature is not supported
    -Usage of domain or TYPE OF COLUMN of array type in PSQL
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
