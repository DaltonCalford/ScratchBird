#coding:utf-8

"""
ID:          issue-8475
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8475
TITLE:       Wrong message about invalid time zone in CAST FORMAT
DESCRIPTION:
NOTES:
    [02.04.2025] pzotov
    1. See also:
        https://github.com/FirebirdSQL/firebird/issues/2388
    2. An issue found, see:
        https://github.com/FirebirdSQL/firebird/issues/8475#issuecomment-2772324636
        (appropriate statement was commented for now)

    Confirmed problem on 6.0.0.687-730aa8f (22-mar-2025).
    Checked on 6.0.0.710-40651f6.
"""
import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set bail OFF;
    set heading off;
    select cast ('2005-03-16 01:02:03.1234 +01:00'as timestamp with time zone format 'YYYY-MM-DD HH24:MI:SS.FF4 TZR') from rdb$database;
    select cast ('12:30 2:30' as time with time zone format 'HH24:MI TZR') from rdb$database;
    select cast ('12:30 -2:30' as time with time zone format 'HH24:MI TZR') from rdb$database;
    select cast ('2:30 0:30 A.M.' as time with time zone format 'HH:MI TZR A.M.') from rdb$database;
    select cast ('00:60' as time with time zone format 'TZH:TZM') from rdb$database;
    select cast ('-9:-99' as time with time zone format 'TZH:TZM') from rdb$database;
    select cast ('-14:01' as time with time zone format 'TZH:TZM') from rdb$database;
    select cast ('12 12' as time with time zone format 'HH24 HH24') from rdb$database;
    select cast ('2025-02-30' as date format 'YYYY-MM-DD') from rdb$database;
    -- COMMENTED TEMPORARY (?), SEE
    -- https://github.com/FirebirdSQL/firebird/issues/8475#issuecomment-2772324636
    -- select cast ('00:2147483648' as time with time zone format 'TZH:TZM') from rdb$database;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        2005-03-16 01:02:03.1234 +01:00

        12:30:00.0000 +02:30

        12:30:00.0000 -02:30

        02:30:00.0000 +00:30

        Statement failed, SQLSTATE = HY000
        Value for TZM pattern is out of range [0, 59]

        Statement failed, SQLSTATE = HY000
        Value for TZM pattern is out of range [0, 59]

        Statement failed, SQLSTATE = 22009
        Invalid time zone offset: -14:01 - must use format +/-hours:minutes and be between -14:00 and +14:00

        Statement failed, SQLSTATE = HY000
        Cannot use the same pattern twice: HH24

        Statement failed, SQLSTATE = 22018
        conversion error from string "2025-02-30"
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
