#coding:utf-8

"""
ID:          issue-1061
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1061
TITLE:       Support for time zones [CORE694]
DESCRIPTION:
    Commit of functionality described in this ticket:
    https://github.com/FirebirdSQL/firebird/commit/71008794984443cfa2417ed1727b9635300e7f6d

    We make two custom DatabaseConfig objects and put different session_time_zone values
    in each of them, "-7:00" and "+7:00" (see 'db_cfg_object' variable).
    Then we make connections using these objects and get current_time from server.
    These two values must differ for 50400 or 50401 seconds.
    -----------------------------------------------------------------------------------------
    ::: ACHTUNG :::
    THIS TEST SHOULD NOT BE CONFUSED WITH CORE_6395_TEST.PY WHICH WAS REMOVED BECAUSE
    FIREBIRD.CONF MUST NOT BE CHANGED // DISCUSSED WITH PCISAR, LETTERS SINCE 31-MAY-2022.

JIRA:        CORE-694
FBTEST:      NOPE
NOTES:
    [14.08.2022] pzotov
    1.Confirmed problem on 4.0.0.1227 (build 29.09.2018): time values were the same.
    2. All fine on 4.0.0.1346 (build 17.12.2018): times differ for required value.
    3. From doc/sql.extensions/README.time_zone.md:
          The session time zone ... can be set (with this priority) using:
          a) `isc_dpb_session_time_zone` DPB,
          b) the client's `firebird.conf` parameter `DefaultTimeZone` and
          c) the server's `firebird.conf` parameter `DefaultTimeZone`.
       This test verifies ability to set session time zone using DPB parameter, i.e. we check
       here ONLY ITEM "a".
       Items "b" and "c" can NOT be checked because it was decided NOT to change firebird.conf
       and/or not to start new FB instance which will use changed config.
       NOTE: item "b" relates to core-6395 (https://github.com/FirebirdSQL/firebird/issues/6633),
       but this test was REMOVED because it can not be implemented.

    4. Checked on 5.0.0.623, 4.0.1.2692 - both on Windows and Linux.
"""

import datetime
import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    
    time_values = []
    for i in (0,1):
        db_cfg_name = f'tmp_0694_{i}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)

        db_cfg_object.database.value = str(act.db.db_path)
        db_cfg_object.session_time_zone.value = '-7:00' if i == 0 else '+7:00'

        check_sql = """
            select
                substring( cast(cast(current_time as time) as varchar(13)) from 1 for 8) as cur_time
            from rdb$database
        """

        with connect(db_cfg_name) as con:
            with con.cursor() as cur:
                for r in cur.execute(check_sql):
                    time_values.append( datetime.datetime.strptime(r[0], '%H:%M:%S') )

    time_diff_seconds = (time_values[1] - time_values[0]).seconds
    msg_prefix = 'Time diff, seconds: '
    if time_diff_seconds in (50400, 50401):
        print(msg_prefix + 'OK')
    else:
        print(msg_prefix + 'WRONG, %d s' % time_diff_seconds)
        print(time_values)

    act.expected_stdout = msg_prefix + 'OK'
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
