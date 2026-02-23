#coding:utf-8

"""
ID:          issue-5551
ISSUE:       5551
TITLE:       Attempt to create database with running trace leads to consistency check (cannot find tip page (165), file: tra.cpp line: 2233)
DESCRIPTION:
    1. Get the content of firebird.log before test.
    2. Make config file and launch trace session, with separate logging of its STDOUT and STDERR.
    3. Make DDLfile and run ISQL, with separate logging of its STDOUT and STDERR.
    4. Stop trace session
    5. Get the content of firebird.log after test.
    6. Ensure that files which should store STDERR results are empty.
    7. Ensure that there is no difference in the content of firebird.log.
JIRA:        CORE-5273
FBTEST:      bugs.core_5273
"""

import pytest
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

temp_db = temp_file('tmp_5273.fdb')

trace = ['time_threshold = 0',
         'log_sweep = true',
         'log_errors = true',
         'log_connections = true',
         'log_statement_prepare = true',
         'log_statement_start = true',
         'log_statement_finish = true',
         'log_statement_free = true',
         'log_trigger_start = true',
         'log_trigger_finish = true',
         'print_perf = true',
         'max_sql_length = 16384',
         'max_log_size = 5000000',
         ]

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, temp_db: Path):
    sql_ddl = f"""
        set list on;
        set bail on;
        create database 'localhost:{temp_db}';
        select mon$database_name from mon$database;
        commit;
        drop database;
    """
    # Get content of firebird.log BEFORE test
    log_before = act.get_firebird_log()

    # Start trace
    with act.trace(db_events=trace, keep_log=False, database=temp_db.name):
        act.isql(switches = ['-q'], input = sql_ddl, connect_db = False, combine_output = True)

    # Get content of firebird.log AFTER test
    log_after = act.get_firebird_log()

    # Check
    assert list(unified_diff(log_before, log_after)) == []
