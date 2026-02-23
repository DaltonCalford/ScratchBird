#coding:utf-8

"""
ID:          issue-5588
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5588
TITLE:       Support full SQL standard binary string literal syntax [CORE5311]
DESCRIPTION:
JIRA:        CORE-5311
FBTEST:      bugs.gh_5588
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set list on;
    select x'01AB' as good_bin_literal_1 from rdb$database;

    select x' 0 1 a b' as good_bin_literal_2 from rdb$database;

    select x'01' 'ab' as good_bin_literal_3 from rdb$database;

    select x'01'/*comment*/'a b' as good_bin_literal_4 from rdb$database;

    select x'01' 'ab' /*comment*/ 'ff00' as good_bin_literal_5 from rdb$database;

    select x'01' -- comment and newline
    'ab' as good_bin_literal_6
    from rdb$database;


    select x'0                                                                                                                                              1' -- comment and newline



    /*
    foo
    --*/
    /**/
    /*
    */





       																											'ab'

    /*
    bar
    --*/


       																																	'cd'
    as good_bin_literal_7
    from rdb$database;

    select x'ab''cd' as poor_bin_literal_1 from rdb$database; -- should not be valid.

"""

act = isql_act('db', test_script, substitutions=[('line\\s+\\d+,\\s+col.*', ''), ('[ \t]+', ' ')])

expected_stdout = """
    GOOD_BIN_LITERAL_1              01AB
    GOOD_BIN_LITERAL_2              01AB
    GOOD_BIN_LITERAL_3              01AB
    GOOD_BIN_LITERAL_4              01AB
    GOOD_BIN_LITERAL_5              01ABFF00
    GOOD_BIN_LITERAL_6              01AB
    GOOD_BIN_LITERAL_7              01ABCD
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 13
    -'cd'
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
