#coding:utf-8

"""
ID:          issue-4704
ISSUE:       4704
TITLE:       User savepoints are not released on commit
DESCRIPTION:
    Added separate code for 4.0: one need to be sure that all changes have been flushed on disk before we launch gstat.
    See letter from hvlad, 02.02.2019 22:30.
    ::: NOTE :::
    !! It looks strange but if we put preparing statement in 'init_script' section than result of 'gstat -i' will be WRONG,
    even if we do db_conn.close() before runProgram('gstat' ...) !!
JIRA:        CORE-4382
NOTES:
    [07.10.2025] pzotov
    Separated expected output for 6.x after commit 1214b460 ("Remove the check in loadInventoryPage ...", see #8542):
    number of nodes become 1 (as it was in 3.x).

    Checked on: 6.0.0.1295; 5.0.4.1711; 4.0.7.3235; 3.0.14.33826.
"""

import pytest
from firebird.qa import *

substitutions = [('^((?!nodes|Error|SQLSTATE).)*$', ''), ('Root page: \\d+,', 'Root page: N'), ('Depth', 'depth')]

db = db_factory()
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    test_script = """
        create table g_test (f integer);
        create index g_ind on g_test (f);
        insert into g_test values (1);
        commit;
        update g_test set f=2;
        savepoint a;
        update g_test set f=3;
        savepoint b;
        update g_test set f=4;
        savepoint c;
        update g_test set f=5;
        savepoint d;
        update g_test set f=6;
        savepoint e;
        update g_test set f=7;
        commit;
        select * from g_test;
        COMMIT;
        -- Confirmed result of "gstat -i"
        -- 1) for 3.0 Alpha1 & Alpha2:
        -- Root page: 203, depth: 1, leaf buckets: 1, nodes: 6
        --                                                   ^- orphans, must be: 1
        -- 2) for 2.5.3:
        -- Depth: 1, leaf buckets: 1, nodes: 6
        -- ^- upper case!                    ^- orphans, must be: 1
    """
    act.isql(switches=['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == '', f'Error in test script:\n{act.clean_stdout}'
    act.reset()

    
    expected_stdout_3x = """
        Root page: 203, depth: 1, leaf buckets: 1, nodes: 1
    """
    expected_stdout_4x = """
        Root page: 203, depth: 1, leaf buckets: 1, nodes: 2
    """

    expected_stdout_6x = """
        Root page: 203, depth: 1, leaf buckets: 1, nodes: 1
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x if act.is_version('<6') else expected_stdout_6x
    act.gstat(switches=['-i'])
    assert act.clean_stdout == act.clean_expected_stdout
