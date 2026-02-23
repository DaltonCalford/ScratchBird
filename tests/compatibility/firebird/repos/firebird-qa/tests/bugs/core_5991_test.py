#coding:utf-8

"""
ID:          issue-6241
ISSUE:       6241
TITLE:       Trace could not work correctly with quoted file names in trace configurations
DESCRIPTION:
    Thank Vlad for suggestions.

    NOTE-1. Bug will NOT appear if PATTERN is used in database-section!
    In order to reproduce bug one need to create config file for trace with following
    _SINGLE_ file name in databases-section:
    =====
        database = 'C:\\FBTESTING\\qa\\fbt-repo\\tmp\\tmp_5991.o'clock.fdb'
        {
            enabled = true
            time_threshold = 0
            log_initfini = false
            log_connections = true
            log_transactions = true
            log_statement_finish = true
        }
    =====
    (path 'C:\\FBTESTING\\qa\\fbt-repo\\tmp' will be replaced with actual test DB location)

    Then we start trace session.

    NOTE-2: if this trace session will be forced to wait about 10 seconds, then error message will appear
    with text "error while parsing trace configuration" but DB name will be securityN.fdb.
    Moreover, an operation with any DB which has name different than specified in database-section will
    raise this error, and its text can be misleading that trace did not started at all or was terminated.
    This is because another bug (not yet fixed) which Vlad mentioned privately in letter 26.02.19 23:37.

    :::: NB :::::
    We can IGNORE this error message despite it contains phrase "Error creating trace session" and go on.
    Trace session actually *WILL* be created and we have to check this here by further actions with DB.
    :::::::::::::

    After this, we create database with the same name by calling fdb.create_database().
    NOTE-3: we have to enclose DB file in double quotes and - moreover - duplicate single apostoph,
    otherwise fdb driver will create DB without it, i.e.: "tmp_5991.oclock.fdb".

    At the second step we do trivial statement and drop this database (tmp_5991.o'clock.fdb).
    Finally, we wait at least two seconds because trace buffer must be flushed to disk, stop trace session
    and then - open trace log for parsing it.
    Trace log MUST contain all of following phrases (each of them must occur in log at least one time):
        1. Trace session ID <N> started
        2. CREATE_DATABASE
        3. START_TRANSACTION
        4. EXECUTE_STATEMENT_FINISH
        5. ROLLBACK_TRANSACTION
        6. DROP_DATABASE
    We check each line of trace for matching to patterns (based on these phrases) and put result into Python dict.
    Resulting dict must contain 'FOUND' and value for every of its keys (patterns).

    Confirmed bug on 3.0.4.33054.
JIRA:        CORE-5991
FBTEST:      bugs.core_5991
NOTES:
    [15.09.2022] pzotov
    Checked on Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730
"""

import pytest
import re
from firebird.qa import *

db = db_factory(filename="core_5991.o'clock.fdb")

act = python_act('db', substitutions=[('Trying to create.*', '')])

allowed_patterns = {'1. DB_ATTACH': re.compile('[.*]*ATTACH_DATABASE\\.*', re.IGNORECASE),
                    '2. TX_START': re.compile('[.*]*START_TRANSACTION\\.*', re.IGNORECASE),
                    '3. STATEMENT_DONE': re.compile('[.*]*EXECUTE_STATEMENT_FINISH\\.*', re.IGNORECASE),
                    '4. TX_FINISH': re.compile('[.*]*ROLLBACK_TRANSACTION\\.*', re.IGNORECASE),
                    '5. DB_DETACH': re.compile('[.*]*DETACH_DATABASE\\.*', re.IGNORECASE),
                    }

expected_stdout = """
    Database name contains single quote.
    Pattern 1. DB_ATTACH : FOUND
    Pattern 2. TX_START : FOUND
    Pattern 3. STATEMENT_DONE : FOUND
    Pattern 4. TX_FINISH : FOUND
    Pattern 5. DB_DETACH : FOUND
"""

trace = ['{',
         'enabled = true',
         'log_connections = true',
         'log_transactions = true',
         'log_statement_finish = true',
         'log_initfini = false',
         'time_threshold = 0',
         '}'
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.5')
def test_1(act: Action, capsys):
    trace.insert(0, f"database = '{act.db.db_path}'")
    with act.trace(config=trace):
        with act.db.connect() as con:
            c = con.cursor()
            for row in c.execute("select 'Database name contains single quote.' as result from mon$database where lower(mon$database_name) similar to '%[\\/](core_5991.o''clock).fdb'"):
                print(row[0])
    # Process trace
    found_patterns = {}
    for line in act.trace_log:
        if line.rstrip().split():
            for key, pattern in allowed_patterns.items():
                if pattern.search(line):
                    found_patterns[key] = 'FOUND'
    for key, status in sorted(found_patterns.items()):
        print(f'Pattern {key} : {status}')

    if len(found_patterns) < len(allowed_patterns):
        print('==== INCOMPLETE TRACE LOG: ====')
        for line in act.trace_log:
            if line.strip():
                print('  ' + line)
        print('=' * 31)
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
