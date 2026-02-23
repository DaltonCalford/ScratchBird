#coding:utf-8

"""
ID:          issue-6013
ISSUE:       6013
TITLE:       Date-time parsing is very weak
DESCRIPTION:
    Parser changed in:
    https://github.com/FirebirdSQL/firebird/commit/ff37d445ce844f991242b1e2c1f96b80a5d1636d
    See also:
        - CORE-6427 - Whitespace as date separator causes conversion error.
        - CORE-6429 - Timezone offset in timestamp/time literal and CAST should follow SQL standard syntax only.
JIRA:        CORE-5750
FBTEST:      bugs.core_5750
NOTES:
    [03.06.2023] pzotov
    If source string contains binary characters (e.g. '2001') then it will not be converted to date,
    but content of error message depends on FB version:
        * on FB 4.x and FB 5.x prior 5.0.0.1058 it will look without changes, i.e. "2001";
        * on 5.0.0.1058 it will look like this: conversion error from string "2029\x01\x19";
        * on 5.0.0.1066 it will look like this: conversion error from string "2029#x01#x19";
    Error text became differ after 30-may-2023:
        https://github.com/FirebirdSQL/firebird/commit/fa6f9196f9015d0cf8c1cb84ff6312934855e9e9
        ("Fixed #7599: Conversion of text with '\0' to DECFLOAT without errors")
    Expected STDERR was adjusted to current output after discuss with Alex.

    [17.06.2023] pzotov
    Since 16.06.2023: output for FB 4.x became identical to FB 5.x, see commit
    https://github.com/FirebirdSQL/firebird/commit/6a1311f1eee2f1a3a2710b0daf900f32aac95d1a

    Checked on 5.0.0.1066, 4.0.3.2948; 4.0.3.2953 (17.06.2023)
"""

import pytest
from firebird.qa import *

db = db_factory()

BINARY_DATA_SUFFIX = ''
test_script = f"""
    set heading off;
    --set echo on;

    -- All these must fail:
    select timestamp '2018-01-01 10 20 30' from rdb$database;
    select timestamp '2018-01-01 10,20,30 40' from rdb$database;
    select timestamp '31.05.2017 1:2:3.4567' from rdb$database;
    select timestamp '31/05/2017 2:3:4.5678' from rdb$database;
    ------------------------------------------------------------

    -- Date components separator may be a single one (first and second occurence): dash, slash or dot.
    select date '2018-01-31' from rdb$database;
    select date '2018/01/31' from rdb$database;
    select date '2018.01.31' from rdb$database;
    select date '2001{BINARY_DATA_SUFFIX}' from rdb$database;
    select date '2018,01,31' from rdb$database;
    select date '2018/01.31' from rdb$database;
    select date '2018 01 31' from rdb$database;

    -- Time components (hours to minutes to seconds) separator may be colon.
    select time '10:29:39' from rdb$database;
    select time '10/29/39' from rdb$database;
    select time '10.29.39' from rdb$database;
    select time '10-29-39' from rdb$database;
    select time '10 29 39' from rdb$database;

    -- Seconds to fractions separator may be dot.
    select time '7:3:1.' from rdb$database;
    select time '7:3:2.1238' from rdb$database;
    select time '7:3:3,1238' from rdb$database;

    -- There could *NOT* be any separator (other than spaces) between date and time.
    select timestamp '2018-01-01T01:02:03.4567' from rdb$database;
    select timestamp '31.05.2017       11:22:33.4455' from rdb$database;
    select timestamp '31.05.2017
    22:33:44.5577' from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    2017-05-31 01:02:03.4567
    2018-01-31
    2018-01-31
    2018-01-31
    2018-01-31
    10:29:39.0000
    07:03:01.0000
    07:03:02.1238
    2017-05-31 11:22:33.4455
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    BINARY_DATA_OUTPUT = '#x01#x19'

    expected_stderr = f"""
        Statement failed, SQLSTATE = 22009
        Invalid time zone region: 20 30

        Statement failed, SQLSTATE = 22009
        Invalid time zone region: ,20,30 40

        Statement failed, SQLSTATE = 22018
        conversion error from string "31/05/2017 2:3:4.5678"

        Statement failed, SQLSTATE = 22018
        conversion error from string "2001{BINARY_DATA_OUTPUT}"

        Statement failed, SQLSTATE = 22018
        conversion error from string "2018,01,31"

        Statement failed, SQLSTATE = 22018
        conversion error from string "2018/01.31"

        Statement failed, SQLSTATE = 22009
        Invalid time zone region: /29/39

        Statement failed, SQLSTATE = 22009
        Invalid time zone region: .39

        Statement failed, SQLSTATE = 22009
        Invalid time zone offset: -29-39 - must use format +/-hours:minutes and be between -14:00 and +14:00

        Statement failed, SQLSTATE = 22009
        Invalid time zone region: 29 39

        Statement failed, SQLSTATE = 22009
        Invalid time zone region: ,1238

        Statement failed, SQLSTATE = 22009
        Invalid time zone region: T01:02:03.4567

        Statement failed, SQLSTATE = 22009
        Invalid time zone region:
            22:33:44.5577
    """

    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
