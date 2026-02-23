#coding:utf-8

"""
ID:          issue-6665
ISSUE:       6665
TITLE:       Whitespace as date separator causes conversion error
DESCRIPTION:
  Only space and TAB are allowed to usage as whitespace.
  Characters like chr(10) or chr(13) are not allowed:
    cast( '01.01.00 03:04:05.678' || ascii_char(10) as timestamp)
  -- leads to "Statement failed, SQLSTATE = 22009 / Invalid time zone region:"
JIRA:        CORE-6427
FBTEST:      bugs.core_6427
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;

    select cast('01 jan	1900' as timestamp) from rdb$database;

    -- NB: max allowed length for string which is to be converted to timestamp is 130:
    select cast('01' || lpad('',120,' ') || 'jan 1900' as timestamp) from rdb$database;
    select cast('01' || lpad('',120,ascii_char(9)) || 'jan 1900' as timestamp) from rdb$database;
    select cast('01 jan' || lpad('',120,ascii_char(9)) || '1900' as timestamp) from rdb$database;

    select cast('01 jan	' || ascii_char(9) || '1900 1:1   ' as timestamp) from rdb$database;
    select cast('01 jan'|| ascii_char(9) || ascii_char(9) || '1900 1:11  ' as timestamp) from rdb$database;
    select cast('01 jan 1900 11:1'|| ascii_char(9) as timestamp) from rdb$database;
    select cast( ascii_char(9) || '01' || ascii_char(9) ||'jan 1900 11:11' || ascii_char(9) as timestamp) from rdb$database;

    select cast('01 jan 00' as timestamp) from rdb$database;
    select cast('01 jan 00 00:00' as timestamp) from rdb$database;

    select cast('01 01 1900' as timestamp) from rdb$database;
    select cast('01 01 00' as timestamp) from rdb$database;
    select cast('1' || ascii_char(9) || '1 0' as timestamp) from rdb$database;
    select cast('1 1' || ascii_char(9) || '1' as timestamp) from rdb$database;
    select cast('1 1' || ascii_char(9) || '9999' as timestamp) from rdb$database;

    select cast('01 01 2000' as timestamp) from rdb$database;
    select cast('12 01 2000' as timestamp) from rdb$database;
    -- conversion error (for unknown reason though...) select cast('13 01 2000' as timestamp) from rdb$database;

    select cast('01 01 00' || ascii_char(9) || '23:2.2' as timestamp) from rdb$database;
    select cast('01 01' || ascii_char(9) || '00' || ascii_char(9) || '3:45:5.9' as timestamp) from rdb$database;

    select cast( ascii_char(9) || '01 01 00' || ascii_char(9) || '3:4:5.678' as timestamp) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    1900-01-01 00:00:00.0000
    1900-01-01 00:00:00.0000
    1900-01-01 00:00:00.0000
    1900-01-01 00:00:00.0000
    1900-01-01 01:01:00.0000
    1900-01-01 01:11:00.0000
    1900-01-01 11:01:00.0000
    1900-01-01 11:11:00.0000
    2000-01-01 00:00:00.0000
    2000-01-01 00:00:00.0000
    1900-01-01 00:00:00.0000
    2000-01-01 00:00:00.0000
    2000-01-01 00:00:00.0000
    2001-01-01 00:00:00.0000
    9999-01-01 00:00:00.0000
    2000-01-01 00:00:00.0000
    2000-12-01 00:00:00.0000
    2000-01-01 23:02:00.2000
    2000-01-01 03:45:05.9000
    2000-01-01 03:04:05.6780
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
