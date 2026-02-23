#coding:utf-8

"""
ID:          issue-8085
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8085
TITLE:       Memory leak when executing a lot of different queries and StatementTimeout > 0
DESCRIPTION:
    Test launches ISQL in async mode and then checks in loop for <N_CNT> seconds value of psutil.Process( <fb_pid> )
    where <fb_pid> is value of server PID that can be found in mon$attachments.mon$server_pid of ISQL connection.
    This value is written in auxiliary table 'tmplog' and can be obtained from it when ISQL establishes attachment.
    Then we collect values of memory_info().rss returned by instance of psutil.Process( <fb_pid> ) in the list,
    see 'memo_rss_list' variable.
    Collection of memory_info().rss value is made with interval 1 second.
    After that we reuire ISQL process to be terminated and wait for that no more than <MAX_WAIT_FOR_ISQL_TERMINATE> seconds.
    Finally, we evaluate differences between adjacent values from memo_rss_list.
    Median of these differences must be <MAX_RSS_DIFFERENCE_MEDIAN> (Kb).
    Before fix this median was about 650K.
NOTES:
    [17.04.2024] pzotov
    Bug detected on 6.0.0.313 during implementation of test for gh-2388 (there is loop with ~20E6 iterations which run ES).
    Confirmed fix on intermediate snapshots: 6.0.0.321 #cc6fe45; 5.0.1.1381 #0f3cdde; 4.0.5.3086 #9d13bd3
"""

import psutil
import pytest
import time
from pathlib import Path
import subprocess

import firebird.driver
from firebird.qa import *

db = db_factory()
act = python_act('db')

N_CNT = 15
tmp_sql = temp_file('tmp_8085.sql')
tmp_log = temp_file('tmp_8085.log')

MAX_WAIT_FOR_ISQL_BEGIN_WORK=3
MAX_WAIT_FOR_ISQL_TERMINATE=11
MAX_RSS_DIFFERENCE_MEDIAN = 0

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.5')
def test_1(act: Action, tmp_sql: Path, tmp_log: Path, capsys):

    test_sql = f"""
        recreate table tmplog(srv_pid int);
        insert into tmplog(srv_pid) 
        select mon$server_pid as p
        from mon$attachments
        where mon$attachment_id = current_connection
        ;
        commit;
        SET STATEMENT TIMEOUT 7200;
        set term ^;
        execute block as
            declare res double precision;
        begin
            while (1=1) do
            begin
                execute statement 'select ' || rand() || ' from rdb$database' into res;
            end
        end
        ^
    """
    with open(tmp_sql, 'w') as f:
        f.write(test_sql)

    memo_rss_list = []
    with act.db.connect() as con:
        with open(tmp_log, 'w') as f:
            try:
                p_handed_isql = subprocess.Popen( [act.vars['isql'], '-i', str(tmp_sql),
                                                  '-user', act.db.user,
                                                  '-password', act.db.password, act.db.dsn],
                                                  stdout = f,
                                                  stderr = subprocess.STDOUT
                                                )

                # Let ISQL time to establish connection and start infinite loop with ES:
                time.sleep(MAX_WAIT_FOR_ISQL_BEGIN_WORK)

                cur = con.cursor()
                cur.execute('select srv_pid from tmplog')
                fb_srv = psutil.Process( int(cur.fetchone()[0]) )

                for i in range(N_CNT):
                    memo_rss_list.append(int(fb_srv.memory_info().rss / 1024))
                    time.sleep(1)

            finally:
                p_handed_isql.terminate()

            p_handed_isql.wait(MAX_WAIT_FOR_ISQL_TERMINATE)
            if p_handed_isql.poll() is None:
                print(f'ISQL process WAS NOT terminated in {MAX_WAIT_FOR_ISQL_TERMINATE} second(s).!')
            else:
                print(f'ISQL process terminated.')
        
        memo_rss_diff = []
        for i,x in enumerate(memo_rss_list):
            if i >= 1:
                memo_rss_diff.append(x - memo_rss_list[i-1])

    memo_rss_diff_median = median(memo_rss_diff)
    median_acceptable_msg = 'Memory differences median acceptable.'
    if memo_rss_diff_median <= MAX_RSS_DIFFERENCE_MEDIAN:
        print(median_acceptable_msg)
    else:
        print(f'Memory LEAK detected. Median of differences: {memo_rss_diff_median} Kb - is UNACCEPTABLE. Check memo_rss_diff:')
        for p in memo_rss_diff:
            print('%6d' % p)

    expected_stdout = f"""
        ISQL process terminated.
        {median_acceptable_msg}
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
