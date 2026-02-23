#coding:utf-8

"""
ID:          issue-3952
ISSUE:       3952
TITLE:       TRACE: add statistics of actions that were after transaction finished
DESCRIPTION:
  Three tables are created: permanent, GTT with on commit PRESERVE rows and on commit DELETE rows.

  Trace config is created with *prohibition* of any activity related to security<N>.fdb
  but allow to log transactions-related events (commits and rollbacks) for working database.
  Trace is started before furthe actions.

  Then we launch ISQL and apply two DML for each of these tables:
  1) insert row + commit;
  2) insert row + rollback.

  Finally (after ISQL will finish), we stop trace and parse its log.
  For *each* table TWO lines with performance statristics must exist: both for COMMIT and ROLLBACK events.
JIRA:        CORE-3598
FBTEST:      bugs.core_3598
NOTES:
    [08.02.2022] pcisar
    Fails on Windows 3.0.8 with unexpected additional output line:
      + Statement statistics detected for ROLLBACK
        Statement statistics detected for COMMIT
        Statement statistics detected for COMMIT
        Statement statistics detected for ROLLBACK
        Found performance block header
        Found table statistics for TFIX
        Statement statistics detected for COMMIT
        Statement statistics detected for ROLLBACK
        Found performance block header
        Found table statistics for GTT_SSN
        Statement statistics detected for COMMIT
        Statement statistics detected for ROLLBACK

    [04.03.2022] pzotov: RESOLVED.
    Problem on Windows was caused by excessive query:
        "select current_user, current_role from rdb$database"
    -- which is done by ISQL 3.x when it gets commands from STDIN via PIPE mechanism.
    Discussed with Alex et al, since 28-feb-2022 18:05 +0300.
    Alex explanation: 28-feb-2022 19:52 +0300
    subj: "Firebird new-QA: weird result for trivial test (outcome depends on presence of... running trace session!)"

    [27.06.2025] pzotov
    Added substitutions to suppress output of "PUBLIC" schema name in expected output (no needed for this test).
    Added check for presence `"PUBLIC".` prefix in thetable statistics for table names that are used in this test.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
import platform
from firebird.qa import *

init_script = """
    recreate table tfix(id int);
    recreate global temporary table gtt_ssn(id int) on commit preserve rows;
    recreate global temporary table gtt_tra(id int) on commit delete rows;
"""

db = db_factory(init=init_script)

substitutions = [('for \"PUBLIC\".\"TFIX\"', 'for TFIX'), ('for \"PUBLIC\".\"GTT_SSN\"', 'for GTT_SSN'), ]

act = python_act('db', substitutions = substitutions)

test_script = """
    set autoddl off;
    set echo on;
    set count on;
    set bail on;
    insert into tfix(id) values(1);
    commit;
    insert into tfix(id) values(2);
    rollback;
    insert into gtt_ssn(id) values(1);
    commit;
    insert into gtt_ssn(id) values(2);
    rollback;
    insert into gtt_tra(id) values(1);
    commit;
    insert into gtt_tra(id) values(2);
    rollback;
"""

trace = ['log_transactions = true',
         'print_perf = true',
         'log_initfini = false',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    expected_stdout = ''
    if platform.system() == 'Windows' and act.is_version('<4.0'):
        # FB 3.0.x, WINDOWS ONLY: 'excessive' query (select current_user,current_role from rdb$database)
        # with further ROLLBACK is performed by ISQL in authentification purpose when commands are passed
        # via PIPE mechanism.
        # We can NOT ignore this query here because trace session is started without our explicit control
        # (in contrary to FBT suite), thus we have to add one more line to expected_stdout here:
        expected_stdout = """
            Statement statistics detected for ROLLBACK
        """

    expected_stdout += """
        Statement statistics detected for COMMIT
        Statement statistics detected for COMMIT
        Statement statistics detected for ROLLBACK
        Found performance block header
        Found table statistics for TFIX
        Statement statistics detected for COMMIT
        Statement statistics detected for ROLLBACK
        Found performance block header
        Found table statistics for GTT_SSN
        Statement statistics detected for COMMIT
        Statement statistics detected for ROLLBACK
    """
    
    with act.trace(db_events=trace):
        act.isql(switches=[], input=test_script)
    # Output log of trace session, with filtering only interested info:
    # Performance header text (all excessive spaces will be removed before comparison - see below):
    perf_header='Table                             Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge'
    checked_events= {') COMMIT_TRANSACTION': 'commit',
                     ') ROLLBACK_TRANSACTION': 'rollback',
                     ') EXECUTE_STATEMENT': 'execute_statement',
                     ') START_TRANSACTION': 'start_transaction'
                     }
    i, k = 0, 0
    watched_event = ''
    for line in act.trace_log:
        k += 1
        e = ''.join([v.upper() for x, v in checked_events.items() if x in line])
        watched_event = e if e else watched_event

        if ' ms,' in line and ('fetch' in line or 'mark' in line): # One of these *always* must be in trace statistics.
            print(f'Statement statistics detected for {watched_event}')
            i += 1
        if ' '.join(line.split()).upper() == ' '.join(perf_header.split()).upper():
            print('Found performance block header')
        if line.startswith('TFIX') or line.startswith('GTT_SSN') or line.startswith('GTT_TRA'):
            print(f'Found table statistics for {line.split()[0]}')

        # Added for FB 6.x: SCHEMA name presents since 6.0.0.834:
        if line.startswith('"PUBLIC"."TFIX"') or line.startswith('"PUBLIC"."GTT_SSN"') or line.startswith('"PUBLIC"."GTT_TRA"'):
            print(f'Found table statistics for {line.split()[0]}')
    
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
