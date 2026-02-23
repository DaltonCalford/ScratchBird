#coding:utf-8

"""
ID:          issue-4604
ISSUE:       4604
TITLE:       FB 3: TYPE OF arguments of stored functions will hang firebird engine if depending domain or column is changed
DESCRIPTION:
JIRA:        CORE-4281
FBTEST:      bugs.core_4281
NOTES:
    [29.06.2025] pzotov
    Removed 'SHOW' command. It is enought to check that function returns proper result after 'ALTE DOMAIN' statement.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create domain dm_test as integer;
    commit;
    set term ^;
    create function fn_test (a_x type of dm_test) returns integer as
    begin
        return sign(a_x);
    end ^
    set term ;^
    commit;
    select fn_test(-2147483648) as fn_neg from rdb$database;
    select fn_test( 2147483648) as fb_pos from rdb$database;
    
    alter domain dm_test type bigint;
    commit;

    select fn_test( 2147483648) as fb_pos from rdb$database;
"""

substitutions = [('[\t ]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    FN_NEG -1
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    FB_POS 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
