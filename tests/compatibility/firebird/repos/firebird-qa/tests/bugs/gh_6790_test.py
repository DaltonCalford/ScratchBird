#coding:utf-8

"""
ID:          issue-6790
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6790
TITLE:       MON$ATTACHMENTS.MON$TIMESTAMP is incorrect when DefaultTimeZone is configured with time zone different from the server's default
DESCRIPTION:
    Test creates custom DatabaseConfig object for writing in its session_time_zone value that will differ from server.
    Query to rdb$time_zones is performed for obtaining random record and assigns it to DefaultTimeZone column of DB config.

    Then we obtain values of current_timestamp and mon$attachments.mon$timestamp for current connection, and parse them in order to:
    * get timezone name for each of these values;
    * get timestamp WITHOUT timezone name.

    Result of parsing for these values must meet following conditions:
    1) both timestamps must belong to the same time zone (this was NOT so before fix);
    2) difference between timestamps must not be valuable, usually this must be no more than 1..2 seconds (see 'MAX_DIFF_MS')
FBTEST:      bugs.gh_6790
NOTES:
    [18.08.2022] pzotov
    Confirmed problem on WI-V4.0.0.2436: either different time zones or too big time differences can be issued.
    Improvement ('add MON$SESSION_TIMEZONE to MON$ATTACHMENTS') commit info:
        06-may-2021 15:18
        https://github.com/FirebirdSQL/firebird/commit/c8750376bb78e5a588fb269a144bba4dea34ae47

    Checked on 5.0.0.623, 4.0.1.2692 - both on Windows and Linux.
"""

import datetime
from datetime import timedelta

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect

MAX_DIFF_MS = 1500

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        with con.cursor() as cur:
            cur.execute('select z.rdb$time_zone_name from rdb$time_zones z order by rand() rows 1')
            RANDOM_TZ = cur.fetchone()[0]

    db_cfg_name = f'tmp_gh_6790'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)

    db_cfg_object.database.value = str(act.db.db_path)
    db_cfg_object.session_time_zone.value = RANDOM_TZ

    sql_chk = f"""
        select
              cast(t1 as varchar(255)) as ts1
             ,cast(t2 as varchar(255)) as ts2
        from (
            select
                 current_timestamp as t1
                ,a.mon$timestamp as t2
            from mon$attachments a
            where a.mon$attachment_id = current_connection
        );
    """

    tz_names_set = set()
    time_values = []
    with connect(db_cfg_name) as con:
        with con.cursor() as cur:
            for r in cur.execute(sql_chk):
                for i,col in enumerate(cur.description):
                    # print((col[0]).ljust(63), r[i])
                    tz_names_set.add(  r[i].split()[-1] )   # '2022-08-18 19:59:48.6860 Pacific/Yap' ==> 'Pacific/Yap'
                    time_values.append(datetime.datetime.strptime(r[i][:23], '%Y-%m-%d %H:%M:%S.%f') )
    
    
    expected_stdout = 'Timestamps difference acceptable.'
    time_diff_ms = (max(time_values) - min(time_values)).total_seconds() * 1000

    if time_diff_ms <= MAX_DIFF_MS and len(tz_names_set) ==1:
        print(expected_stdout)
    else:
        print(f'UNEXPECTED: detected either diff time zones or too significant difference between timestamps.')
        print('1. Check tz_names_set:')
        for p in tz_names_set:
            print(p)
        print(f'2. Check time_values (difference: {time_diff_ms} ms)')
        for p in time_values:
            print(p)


    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
