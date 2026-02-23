#coding:utf-8

"""
ID:          issue-7288
ISSUE:       7288
TITLE:       Server crashes when working with DSQL savepoints
DESCRIPTION:
NOTES:
    [20.02.2023] pzotov
    Confirmed crahses on 5.0.0.698
    Checked on 5.0.0.733 -- all fine.

    [24.06.2025] pzotov
    Fixed wrong value of charset that was used to connect: "utf-8". This caused crash of isql in recent 6.x.
    https://github.com/FirebirdSQL/firebird/commit/5b41342b169e0d79d63b8d2fdbc033061323fa1b
    Thanks to Vlad for solved problem.
"""
import pytest
from firebird.qa import *
from pathlib import Path

db = db_factory(do_not_create=True, do_not_drop = True)

substitutions = [('[ \t]+', ' ')]

act = python_act('db', substitutions = substitutions)

db_tmp = temp_file('gh_7288.tmp.fdb')

@pytest.mark.version('>=3.0.11')
def test_1(act: Action, db_tmp: Path):

    chk_sql = f"""
        set list on;
        -- NB: do NOT use 'set bail on' here!
        create database 'localhost:{db_tmp}' user {act.db.user} password '{act.db.password}';
        savepoint A1;
        release savepoint A1 only;
        select count(*) from rdb$database;
        drop database;
        -----------------------------------------------------
        rollback;
        create database 'localhost:{db_tmp}' user {act.db.user} password '{act.db.password}';
        savepoint A1;
        savepoint A1;
        drop database;
    """

    expected_stdout = "COUNT 1"
    act.isql(switches=['-q'], input = chk_sql, charset='utf8', io_enc='utf8', connect_db = False, credentials = False, combine_output = True)
    assert act.clean_stdout == expected_stdout
