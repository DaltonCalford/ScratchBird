#coding:utf-8

"""
ID:          issue-5763
ISSUE:       5763
TITLE:       Creating a column of type BLOB SUB_TYPE BINARY fails with a Token unknown
DESCRIPTION:
JIRA:        CORE-5494
FBTEST:      bugs.core_5494
NOTES:
    [01.07.2025] pzotov
    Refactored: we have to check only rows which contain either 'sqltype' or 'SQLSTATE'.
    Added appropriate substitutions.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    recreate table test (
        c1 binary(8),
        v1 varbinary(8),
        b1 blob sub_type binary
    );

    insert into test(c1, b1) values('', '');
    update test set c1 = rdb$db_key, v1 = rdb$db_key, b1 = rpad('',32765,gen_uuid());

    set sqlda_display on;
    set planonly;
    select * from test;
"""

substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 8 charset: 1 OCTETS
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 8 charset: 1 OCTETS
    03: sqltype: 520 BLOB Nullable scale: 0 subtype: 0 len: 8
"""

expected_stdout_6x = """
    01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 8 charset: 1 SYSTEM.OCTETS
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 8 charset: 1 SYSTEM.OCTETS
    03: sqltype: 520 BLOB Nullable scale: 0 subtype: 0 len: 8
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
