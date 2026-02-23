#coding:utf-8

"""
ID:          issue-2165
ISSUE:       2165
TITLE:       Field subtype of DEC_FIXED columns not returned by isc_info_sql_sub_type
DESCRIPTION:
    When requesting the subtype of a NUMERIC or DECIMAL column with precision in [19, 34]
    using isc_info_sql_sub_type, it always returns 0, instead of 1 for NUMERIC and 2 for DECIMAL.
JIRA:        CORE-5728
FBTEST:      bugs.core_5728
NOTES:
    [30.10.2019] pzotov
        Adjusted expected-stdout to current FB, new datatype was introduced: numeric(38).
    [25.06.2020] pzotov
        4.0.0.2076: type in SQLDA was changed from numeric to int128 
        (adjusted output after discussion with Alex about CORE-6342).
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(
        distance_num_1 numeric(19)
       ,distance_num_2 numeric(34)
       ,distance_dec_1 decimal(19)
       ,distance_dec_2 decimal(34)
    );
    commit;

    set sqlda_display on;
    select * from test;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 1 len: 16
    02: sqltype: 32752 INT128 Nullable scale: 0 subtype: 1 len: 16
    03: sqltype: 32752 INT128 Nullable scale: 0 subtype: 2 len: 16
    04: sqltype: 32752 INT128 Nullable scale: 0 subtype: 2 len: 16
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
