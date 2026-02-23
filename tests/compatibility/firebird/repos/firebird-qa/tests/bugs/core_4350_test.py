#coding:utf-8

"""
ID:          issue-4672
ISSUE:       4672
TITLE:       Support the SQL Standard ALTER SEQUENCE .. RESTART (without WITH clause)
DESCRIPTION:
JIRA:        CORE-4350
FBTEST:      bugs.core_4350
NOTES:
    [18.08.2020]
    FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
    statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
    gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
    See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
    This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt
    Because of this, it was decided to make separate section for check results of FB 4.x

    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate sequence g1 start with 9223372036854775807 increment by -2147483647;
    recreate sequence g2 start with -9223372036854775808 increment by 2147483647;
    recreate sequence g3 start with 9223372036854775807 increment by  2147483647;
    recreate sequence g4 start with -9223372036854775808 increment by -2147483647;
    commit;
    set list on;
    select next value for g1, next value for g2, next value for g3, next value for g4
    from rdb$database;

    alter sequence g1 restart;
    alter sequence g2 restart;
    alter sequence g3 restart;
    alter sequence g4 restart;
    commit;

    show sequ;
"""

act = isql_act('db', test_script, substitutions=[('=.*', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_2(act: Action):

    expected_stdout_3x = """
        NEXT_VALUE                      9223372034707292160
        NEXT_VALUE                      -9223372034707292161
        NEXT_VALUE                      -9223372034707292162
        NEXT_VALUE                      9223372034707292161

        Generator G1, current value: 9223372036854775807, initial value: 9223372036854775807, increment: -2147483647
        Generator G2, current value: -9223372036854775808, initial value: -9223372036854775808, increment: 2147483647
        Generator G3, current value: 9223372036854775807, initial value: 9223372036854775807, increment: 2147483647
        Generator G4, current value: -9223372036854775808, initial value: -9223372036854775808, increment: -2147483647
    """


    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PUBLIC.'
    expected_stdout_4x = f"""
        NEXT_VALUE                      9223372036854775807
        NEXT_VALUE                      -9223372036854775808
        NEXT_VALUE                      9223372036854775807
        NEXT_VALUE                      -9223372036854775808
        Generator {SQL_SCHEMA_PREFIX}G1, current value: -9223372034707292162, initial value: 9223372036854775807, increment: -2147483647
        Generator {SQL_SCHEMA_PREFIX}G2, current value: 9223372034707292161, initial value: -9223372036854775808, increment: 2147483647
        Generator {SQL_SCHEMA_PREFIX}G3, current value: 9223372034707292160, initial value: 9223372036854775807, increment: 2147483647
        Generator {SQL_SCHEMA_PREFIX}G4, current value: -9223372034707292161, initial value: -9223372036854775808, increment: -2147483647
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

