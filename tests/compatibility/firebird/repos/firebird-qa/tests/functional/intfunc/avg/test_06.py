#coding:utf-8

"""
ID:          intfunc.avg-06
TITLE:       AVG - Integer OverFlow
DESCRIPTION:
FBTEST:      functional.intfunc.avg.06
NOTES:
    [25.06.2020] 4.0.0.2076: changed types in SQLDA from numeric to int128 // after discuss with Alex about CORE-6342.
    [09.07.2020], 4.0.0.2091:
        NO more overflow since INT128 was introduced. AVG() is evaluated successfully.
        Removed error message from expected_stderr, added result into expected_stdout.
    [27.07.2021]
        Changed sqltype in FB 4.x+ to 580 INT64: this is needed since fix #6874.
    [16.12.2023]
        Replaced splitted code with assigning appropiate expected text using if-else depending on act.is_version result.
        Adjusted substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test( id integer not null);
    insert into test values(2100000000);
    insert into test values(2100000000);
    insert into test values(2100000000);
    insert into test values(2100000000);
    commit;
    create or alter view v_test as select avg(2100000000*id)as avg_result from test;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set sqlda_display on;
    select * from v_test;
"""

# Statement failed, SQLSTATE = 22003
# Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype|AVG_RESULT|[Ii]nteger|overflow).)*$', ''), ('[ \t]+', ' ')])

expected_fb3x = """
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    : name: AVG_RESULT alias: AVG_RESULT
    Statement failed, SQLSTATE = 22003
    Integer overflow. The result of an integer operation caused the most significant bit of the result to carry.
"""

expected_fb4x = """
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    : name: AVG_RESULT alias: AVG_RESULT
    AVG_RESULT 4410000000000000000
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_fb3x if act.is_version('<4') else expected_fb4x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
