#coding:utf-8

"""
ID:          issue-3977
ISSUE:       3977
TITLE:       MON$IO_STATS doesn't report page writes performed asynchronously (at the AST level)
DESCRIPTION:
  Thanks to dimitr for suggestions about how this test can be implemented.
  We have to read some part of data from table by "att_watcher", make small change in this table
  by "att_worker" and finaly do commit + once again read this table in "att_watcher".
  Counter mon$page_writes that will be obtained twise by "att_watcher" (before its two changes)
  must differ.

  ::: NOTE-1 :::
  We have to analyze counter mon$page_writes for attachment that is called "att_watcher" here,
  despite that it does NOT change any in queried table!

  ::: NOTE-2 :::
  Superserver should *not* be processed by this test because page cache is shared between all
  attachments thus counter mon$page_writes is NOT changed in this scenario.
  For this reason in SS we can only "simulate" proper outcome.
JIRA:        CORE-3625
FBTEST:      bugs.core_3625
"""

import pytest
from firebird.qa import *

init_script = """
    recreate view v_check as
    select i.mon$page_writes as iostat_pg_writes
    from mon$attachments a
    left join mon$io_stats i on a.mon$stat_id = i.mon$stat_id
    where
        a.mon$attachment_id <> current_connection
        and a.mon$remote_protocol is not null
        and i.mon$stat_group = 1 -- <<< ATTACHMENTS level
    ;

    recreate table test(x int) ;
    insert into test(x) values(1) ;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as worker_con, act.db.connect() as watcher_con:
        watcher = watcher_con.cursor()
        if act.vars['server-arch'] == 'SuperServer':
            # SUPERSERVER SHOULD *NOT* BE PROCESSED BY THIS TEST
            # COUNTER MON$PAGE_WRITES IS NOT CHANGED DURING RUN,
            pytest.skip("Does not apply to SuperServer")
        else:
            # Do following in connection-WATCHER:
            # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            worker_con.execute_immediate('update test set x = 2 rows 1')

            watcher.execute('select * from v_check') # get FIRST value of mon$page_writes
            page_writes_at_point_1 = watcher.fetchone()[0]

            # Again do in connection-worker: add small change to the data,
            # otherwise watcher will not get any difference in mon$page_writes:
            worker_con.execute_immediate('update test set x = 3 rows 1')

            watcher.execute('select * from test')
            watcher.fetchall()

            watcher_con.commit()
            watcher.execute('select * from v_check') # get SECOND value of mon$page_writes
            page_writes_at_point_2 = watcher.fetchone()[0]
            # PAGE_WRITES DIFFERENCE SIGN: 1
            assert abs(page_writes_at_point_2 - page_writes_at_point_1) == 1
