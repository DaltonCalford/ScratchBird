#coding:utf-8

"""
ID:          issue-6528
ISSUE:       6528
TITLE:       Make usage of TIMESTAMP/TIME WITH TIME ZONE convenient for users when appropriate ICU library is not installed on the client side
DESCRIPTION:
    Test only verifies ability to use 'EXTENDED' clause in SET BIND statement.
    We can not simulate absense of appropriate ICU library and for this reason values of time/timestamp are suppressednot checked.
JIRA:        CORE-6286
FBTEST:      bugs.core_6286
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;

    set bind of time with time zone to extended;
    select time '11:11:11.111 Indian/Cocos' as "check_bind_time_with_zone_to_extended" from rdb$database;

    set bind of timestamp with time zone to extended;
    select timestamp '2018-12-31 12:31:42.543 Pacific/Fiji' as "check_bind_timestamp_with_zone_to_extended" from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype|extended)).)*$', ''),
                                                 ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 32750 EXTENDED TIME WITH TIME ZONE scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: check_bind_time_with_zone_to_extended
    check_bind_time_with_zone_to_extended

    01: sqltype: 32748 EXTENDED TIMESTAMP WITH TIME ZONE scale: 0 subtype: 0 len: 12
      :  name: CONSTANT  alias: check_bind_timestamp_with_zone_to_extended
    check_bind_timestamp_with_zone_to_extended
"""

@pytest.mark.version('>=4.0.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
