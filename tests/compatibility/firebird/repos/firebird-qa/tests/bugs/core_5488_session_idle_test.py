#coding:utf-8

"""
ID:          issue-5758-B
ISSUE:       5758
TITLE:       Timeouts for running SQL statements and idle connections
DESCRIPTION: Timeout for IDLE connection (SET SESSION IDLE TIMEOUT <N>)
    We create .sql script with
    1) SET SESSION IDLE TIMEOUT with small delay N (in seconds);
    2) two trivial statements that are separated by artificial delay with T > N.
       Both statements are trivial: select <literal> from rdb$database.

    This delay is done by isql 'SHELL' command and its form depens on OS:
    * shell ping 127.0.0.1 -- for Windows
    * shell sleep -- for Linux.

    Before .sql we launch trace with logging events for parsing them at final stage of test.

    When this .sql script is launched and falls into delay, session timeout must be triggered
    and second statement should raise exception.
    We redirect ISQL output to separate logs and expect that:
    * log of STDOUT contains all except result of 2nd statement (which must fail);
    * log of STDERR contains exception SQLSTATE = 08003 / connection shutdown / -Idle timeout expired

    Trace log should contain only following events:
      * attach to DB
      * start Tx
      * EXECUTE_STATEMENT_FINISH of 'SET TRANSACTION' statement // it appeared in 5.0.0.391, 08-feb-2022
      * EXECUTE_STATEMENT_FINISH of 'set session idle timeout 1 second' statement
      * EXECUTE_STATEMENT_FINISH finish of 'select 1 as point_1 from rdb$database' statement
      * rollback Tx
      * detach DB.


    ::: NB:::
    No events related to SECOND statement should be in the trace log.
NOTES:
[09.02.2022] pcisar
  Have to add ('quit;', '') to substitutions, because this command is echoed to stdout on Windows,
  while it's not on Linux.
JIRA:        CORE-5488
FBTEST:      bugs.core_5488_session_idle
"""

import pytest
import os
import re
from firebird.qa import *

substitutions = [('timeout .* second', 'timeout second'), ('quit;', ''),
                 ('.*After line [\\d]+.*', ''), ('.*shell.*', '')]

db = db_factory()

act = python_act('db', substitutions=substitutions)

expected_trace = """
ATTACH_DATABASE
START_TRANSACTION
EXECUTE_STATEMENT_FINISH
EXECUTE_STATEMENT_FINISH
set session idle timeout second
EXECUTE_STATEMENT_FINISH
ROLLBACK_TRANSACTION
DETACH_DATABASE
"""

expected_stdout = """
set session idle timeout second;
select 1 as point_1 from rdb$database;
POINT_1                         1
select 2 as point_2 from rdb$database;
"""

expected_stderr = """
Statement failed, SQLSTATE = 08003
connection shutdown
-Idle timeout expired.
"""

trace = ['time_threshold = 0',
         'log_initfini = false',
         'log_errors = true',
         'log_connections = true',
         'log_transactions = true',
         'log_statement_finish = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    trace_dts_pattern = re.compile('.*(ATTACH_DATABASE|START_TRANSACTION|EXECUTE_STATEMENT_FINISH|ROLLBACK_TRANSACTION|DETACH_DATABASE)')

    session_idle_timeout = 1
    shell_sleep_sec = session_idle_timeout + 2
    if os.name == 'nt':
        shell_sleep_cmd = f'shell ping -n {shell_sleep_sec} 127.0.0.1 > {os.devnull}'
    else:
        shell_sleep_cmd = f'shell sleep {shell_sleep_sec} > {os.devnull}'

    sql = f"""
        set list on;
        set bail on;
        set echo on;

        set session idle timeout {session_idle_timeout} second;

        select 1 as point_1 from rdb$database;
        {shell_sleep_cmd};
        select 2 as point_2 from rdb$database;
        quit;
        """
    # Trace
    with act.trace(db_events=trace):
        act.expected_stderr = expected_stderr
        act.expected_stdout = expected_stdout
        act.isql(switches=['-q', '-n'], input=sql)
    # Check
    for line in act.trace_log:
        if trace_dts_pattern.match(line):
            print(line.strip().split()[-1])
        if 'set session idle' in line:
            # Since fixed CORE-6469 ("Provide ability to see in the trace log actions related to session management"), 20.01.2021:
            # https://github.com/FirebirdSQL/firebird/commit/a65f19f8b36384d59a55fbb6e0a43a6b84cf4978
            print(line)
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
    act.reset()
    act.expected_stdout = expected_trace
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
