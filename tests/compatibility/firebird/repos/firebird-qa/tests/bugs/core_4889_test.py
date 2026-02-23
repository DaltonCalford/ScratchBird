#coding:utf-8

"""
ID:          issue-5183
ISSUE:       5183
TITLE:       FBSVCMGR with `action_trace_start` prevents in 3.0 SuperServer from connecting using local protocol
DESCRIPTION:
NOTES:
  This test fails on Windows with "FAILED to find text in trace related to EMBEDDED connect."

[08.03.2022] pzotov
  Fail on Windows caused by 'select current_user from rdb$database' which issues '0 records' (instead of expected '1 records')
  But there is no much sence to search this line ('... records fetched') because it is enough to detect only lines with
  ATTACH and DETACH that were performed by embedded connect. Replaced code which did trace log parsing.

JIRA:        CORE-4889
FBTEST:      bugs.core_4889
"""

import pytest
import platform
from firebird.qa import *

db = db_factory()

act = python_act('db')

isq_script = """
set list on;
set count on;
select
    iif(a.mon$remote_protocol is null, 'internal', 'remote') as connection_protocol,
    iif(a.mon$remote_process is null,  'internal', 'remote') as connection_process,
    iif(a.mon$remote_pid     is null,  'internal', 'remote') as connection_remote_pid,
    a.mon$auth_method as auth_method -- should be: 'User name in DPB'
from rdb$database r
left join mon$attachments a on a.mon$attachment_id = current_connection and a.mon$system_flag = 0;
commit;
"""

expected_stdout = """
     Found embedded ATTACH
     Found embedded DETACH
     CONNECTION_PROTOCOL             internal
     CONNECTION_PROCESS              internal
     CONNECTION_REMOTE_PID           internal
     AUTH_METHOD                     User name in DPB
     Records affected: 1
"""

trace = [
         'log_connections = true',
         'log_initfini = false',
         'log_errors = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.trace(db_events=trace):
        act.isql(switches=['-n', '-user', 'tmp$no$such$user$4889', str(act.db.db_path)],
                 connect_db=False, credentials=False, input=isq_script)
    
    # Process trace log
    lprev = ''
    for line in act.trace_log:
        if 'TMP$NO$SUCH$USER$4889:' in line:
            if ') ATTACH_DATABASE' in lprev:
                print('Found embedded ATTACH')
            elif ') DETACH_DATABASE' in lprev:
                print('Found embedded DETACH')
        lprev = line

    print(act.stdout if act.stdout else "FAILED to print log from EMBEDDED connect: log is EMPTY.")
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
