#coding:utf-8

"""
ID:          issue-5749
ISSUE:       5749
TITLE:       Token unknown error on formfeed in query [CORE5479]
DESCRIPTION:
    Test makes temporary .sql file and writes there several queries like:
        select 'some_text'<DELIMITER>from rdb$database;
    - where <DELIMITER> is every character from following list:
        findstr /i /c:"CHR_WHITE," %fb_sources_home%/master/src/dsql/chars.h
    Output:
        009     CHR_WHITE,  // 0x9
        010     CHR_WHITE,  // 0xA
        012     CHR_WHITE,  // 0xC
        013     CHR_WHITE,  // 0xD
        032     CHR_WHITE,  // this can be skipped from check

    NB: character \u000B [currently] NOT present in this list.

NOTES:
    [18.02.2023] pzotov
    Confirmed problem on 5.0.0.736 (18-sep-2022): literal 0xC ('\u000c') could not be used as delimiter, got:
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Token unknown - line 1, column 15
        -
    Checked on 5.0.0.742 - all OK.

    [24.06.2025] pzotov
    Fixed wrong value of charset that was used to connect: "utf-8". This caused crash of isql in recent 6.x.
    https://github.com/FirebirdSQL/firebird/commit/5b41342b169e0d79d63b8d2fdbc033061323fa1b
    Thanks to Vlad for solved problem.
"""

import pytest
from firebird.qa import *
from pathlib import Path

db = db_factory()
act = python_act('db')

tmp_file = temp_file('gh_5749_tmp.sql')

expected_stdout = """
    CONSTANT                        u0009
    CONSTANT                        u000A
    CONSTANT                        u000C
    CONSTANT                        u000D
"""

whitespace_sql = u"""set list on;
select 'u0009'\u0009from rdb$database;
select 'u000A'\u000Afrom rdb$database;
select 'u000C'\u000Cfrom rdb$database;
select 'u000D'\u000Dfrom rdb$database;
"""
# NB: select 'u000B'\u000Bfrom rdb$database; -- FAILS with token unknown.

@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_file: Path):

    tmp_file.write_bytes(whitespace_sql.encode('utf-8'))
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input_file = tmp_file, charset = 'utf8', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
