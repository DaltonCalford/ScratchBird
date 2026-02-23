#coding:utf-8

"""
ID:          issue-6609
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6609
TITLE:       Memory leak at server, when client select computed field, which used COALESCE or CAST [CORE6370]
DESCRIPTION:
    Problem can be seen if we run query to mon$memory_usage before and after loop with statement described in the ticket.
    It is important that on every iteration of loop cursor will be re-created and closed after statement execution (see 'cur2').
    Ratio between values of mon$memory_usage was about 1.63 (for CS) and 1.26 (for SS) before fix.
    After fix these values reduced to ~1.00 (for 3.x and 4.x), but for 5.x+ CS it is about 1.10.
    NOTE: for Classic we have to compare mon$memory_usage that corresponds to ATTACHMENT level (see 'MON_QUERY' variable).
NOTES:
    [18.05.2024] pzotov
    Confirmed bug on 3.0.7.33348 (17-JUL-2020), mon$memo_used ratio for CS: 1.63; SS: 1.26
    Bug was fixed in: dde597e6ae8bbaac45df4f8b38faa9583cd946d4 (27-JUL-2020).
    Checked on:
        3.0.7.33350, mon$memo_used ratio for CS: 1.01; SS: 1.00
        4.0.5.3099,  mon$memo_used ratio for CS: 1.01; SS: 1.00
        5.0.1.1399,  mon$memo_used ratio for CS: 1.11; SS: 1.02
        6.0.0.351,   mon$memo_used ratio for CS: 1.11; SS: 1.02

    [24.12.2024] pzotov
    Separated definition of max ratio thresholds according to FB fork ('standard', 'HQbird', 'RedDatabase').
    Requested by Anton Zuev, RedBase.
"""

import pytest
import platform
from firebird.qa import *

N_CNT = 30000

init_ddl = """
    recreate table tab1 (
        a1 varchar(99),
        a2 varchar(199),
        a3 computed by (coalesce(a1, '')||'-'||coalesce(a2, ''))
    );
"""

db = db_factory(init = init_ddl)
act = python_act('db')

@pytest.mark.version('>=3.0.0')
def test_1(act: Action, capsys):

    mon_memo_beg = 1
    mon_memo_end = 9999999
    with act.db.connect() as con:

        # con.info.server_version output examples:
        # Standard:    'WI-V6.3.2.1580 Firebird 5.0 7961de2'
        # HQbird:      'WI-V6.3.2.1575 Firebird 5.0 HQbird'
        # RedDatabase: 'LI-V6.3.2.0 RedDatabase 5.0 SNAPSHOT.15 (<sha>)'
        #
        fb_vers_txt = con.info.server_version
        if 'RedDatabase' in fb_vers_txt:
            fb_vers_key = 'red'
        elif 'HQbird' in fb_vers_txt:
            fb_vers_key = 'hqb'
        else:
            fb_vers_key = 'std'

        ###############################
        ###   T H R E S H O L D S   ###
        ###############################
        # Requested by Anton Zuev, RedBase:
        max_memo_ratios_map = {
            ('std', 'SuperServer')  : 1.1
           ,('hqb', 'SuperServer')  : 1.1
           ,('red', 'SuperServer')  : 1.1
           ,('std', 'Classic')      : 1.20
           ,('hqb', 'Classic')      : 1.20
           ,('red', 'Classic')      : 1.30
        }
        
        fb_mode = act.get_server_architecture()
        MAX_THRESHOLD = max_memo_ratios_map[ fb_vers_key, fb_mode]

        if fb_mode == 'SuperServer':
            MON_QUERY = 'select mon$memory_used from mon$memory_usage where mon$stat_group = 0'
        else:
            MON_QUERY = """
                select m.mon$memory_used
                from mon$attachments a
                join mon$memory_usage m on a.mon$stat_id = m.mon$stat_id
                where a.mon$attachment_id = current_connection and m.mon$stat_group  = 1;
            """

        cur = con.cursor()
        cur.execute(MON_QUERY)
        mon_memo_beg = int(cur.fetchone()[0])
        con.commit()

        for i in range(N_CNT):
            cur2 = con.cursor()
            cur2.execute(f"select /* iter {i+1} */ t.a3 from tab1 t")
            for r in cur2:
                pass
            cur2.close()

        con.commit()
        cur.execute(MON_QUERY)
        mon_memo_end = int(cur.fetchone()[0])

    msg_ok = 'Memory usage: EXPECTED'
    if mon_memo_end / mon_memo_beg <= MAX_THRESHOLD:
        print(msg_ok)
    else:
        print(f'Memory usage UNEXPECTED: {mon_memo_end} / {mon_memo_beg} = {mon_memo_end / mon_memo_beg:.2f} - greater than {MAX_THRESHOLD=}')
    act.expected_stdout = msg_ok
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
