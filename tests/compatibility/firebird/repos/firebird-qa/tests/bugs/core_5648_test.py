#coding:utf-8

"""
ID:          issue-5914
ISSUE:       5914
TITLE:       Avoid serialization of isc_attach_database calls issued by EXECUTE STATEMENT implementation
DESCRIPTION:
  We use special IP = 192.0.2.2 as never reachable address thus any attempt to connect it will fail.
  Currently FB tries to establish connect to this host about 20-22 seconds.
  We launch 1st ISQL in Async mode (using subprocess.Popen) with task to establish connect to this host.
  At the same time we launch 2nd ISQL with EDS to localhost and the same DB as test uses.
  Second ISQL must do its job instantly, despite of hanging 1st ISQl, and time for this is about 50 ms.
  We use threshold and compare time for which 2nd ISQL did its job. Finally, we ouptput result of this comparison.
NOTES:
  As of current FB snapshots, there is NOT ability to interrupt ISQL which tries to make connect to 192.0.2.2,
  until this ISQL __itself__ make decision that host is unreachable. This takes about 20-22 seconds.
  Also, if we kill this (hanging) ISQL process, than we will not be able to drop database until this time exceed.
  For this reason, it was decided not only to kill ISQL but also run fbsvcmgr with DB full-shutdown command - this
  will ensure that database is really free from any attachments and can be dropped.

  See also #5875 (CORE-5609)
JIRA:        CORE-5648
FBTEST:      bugs.core_5648
"""

import pytest
import subprocess
import time
from pathlib import Path
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod

db = db_factory()

act = python_act('db')

expected_stdout = """
    RESULT_MSG                      OK: second EDS was fast
"""

eds_script = temp_file('eds_script.sql')

@pytest.mark.es_eds
@pytest.mark.version('>=3.0.3')
def test_1(act: Action, eds_script: Path):
    eds_sql = f"""
    set bail on;
    set list on;
    --select current_timestamp as " " from rdb$database;
    set term ^;
    execute block as
        declare c smallint;
        declare remote_host varchar(50) = '%(remote_host)s'; -- never unreachable: 192.0.2.2
    begin
        rdb$set_context('USER_SESSION','DTS_BEG', cast('now' as timestamp) );
        execute statement 'select 1 from rdb$database'
        on external remote_host || ':' || rdb$get_context('SYSTEM', 'DB_NAME')
        as user '{act.db.user}' password '{act.db.password}'
        into c;
    end
    ^
    set term ;^
    --select current_timestamp as " " from rdb$database;
    select iif( waited_ms < max_wait_ms,
                'OK: second EDS was fast',
                'FAILED: second EDS waited too long, ' || waited_ms || ' ms - more than max_wait_ms='||max_wait_ms
              ) as result_msg
    from (
        select
            datediff( millisecond from cast( rdb$get_context('USER_SESSION','DTS_BEG') as timestamp) to current_timestamp ) as waited_ms
           ,500 as max_wait_ms
        --   ^
        --   |                                  #################
        --   +--------------------------------  T H R E S H O L D
        --                                      #################
        from rdb$database
    );
    """
    #
    remote_host = '192.0.2.2'
    eds_script.write_text(eds_sql % locals())
    p_unavail_host = subprocess.Popen([act.vars['isql'], '-n', '-i', str(eds_script),
                                       '-user', act.db.user,
                                       '-password', act.db.password, act.db.dsn],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        time.sleep(2)
        remote_host = 'localhost'
        act.expected_stdout = expected_stdout
        act.isql(switches=['-n'], input=eds_sql % locals())
    finally:
        p_unavail_host.terminate()
    # Ensure that database is not busy
    with act.connect_server() as srv:
        srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                              method=ShutdownMethod.FORCED, timeout=0)
        srv.database.bring_online(database=act.db.db_path)
    # Check
    assert act.clean_stdout == act.clean_expected_stdout
