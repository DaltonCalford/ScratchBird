#coding:utf-8

"""
ID:          issue-4633
ISSUE:       4633
TITLE:       DateAdd(): change input <amount> argument from INT to BIGINT
DESCRIPTION:
JIRA:        CORE-4310
FBTEST:      bugs.core_4310
NOTES:
    [12.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set planonly;
    select dateadd( ? millisecond to ?) from rdb$database;
    set planonly;
    set plan off;
    set sqlda_display off;
    set list on;

    select
         dateadd(  315537897599998 millisecond to timestamp '01.01.0001 00:00:00.001' ) dts1
        ,dateadd( -315537897599998 millisecond to timestamp '31.12.9999 23:59:59.999' ) dts2
    from rdb$database;
"""

act = isql_act('db', test_script,
                 substitutions=[('^((?!SQLSTATE|sqltype|DTS1|DTS2|SQLSTATE|exceed|range|valid).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 580 INT64 scale: -1 subtype: 0 len: 8
    02: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
    01: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
    DTS1                            9999-12-31 23:59:59.9990
    DTS2                            0001-01-01 00:00:00.0010
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

