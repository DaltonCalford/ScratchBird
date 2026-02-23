#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2510
TITLE:       Parts of RDB$DB_KEY of views may be inverted when using FULL JOINs.
DESCRIPTION:
    Test verifies that rdb$db_key of the VIEW that is declared as FULL JOIN of two tables (t1 and t2)
    meets requirement: view_db_key == coalesce(t2.db_key, '0'*16) || coalesce(t1.db_key, '0'*16).
    If any record does not pass this check, we add appropriate db_key values to the special list 'failed_chk'
    which content will be displayed at final point.
    Test can be considered as 'passed' if list 'failed_chk' is empty (i.e. output is empty).
NOTES:
    [20.02.2026] pzotov
    Re-implemented: FULL JOIN execution plan has changed since 19.02.2026 6.0.0.1458,
    commit: "6a76c1da Better index usage in full outer joins...".
    This caused to changed order of output lines.

    We have to check that 'top-level' view rdb$db_key equals to result of concatenation of rdb$db_key
    columns of underlying tables. But concrete values of rdb$db_key NO matter for this test.

    Checked on 6.0.0.1461-5e98812; 6.0.0.1454-b45aa4e; 5.0.4.1767-52823f5; 4.0.7.3243; 3.0.14.33838
"""
import pytest
from firebird.qa import *

init_script = """
    create table t1 (n integer);
    create table t2 (n integer);

    insert into t1 values (1);
    insert into t1 values (2);
    insert into t1 values (3);
    insert into t1 values (4);
    insert into t1 values (5);
    insert into t1 values (6);

    insert into t2 values (2);
    insert into t2 values (4);
    insert into t2 values (5);
    insert into t2 values (8);
    commit;

    create view v (t1, t2) as
    select t1.rdb$db_key, t2.rdb$db_key
    from t1
    full join t2 on t2.n = t1.n
    ;
"""

db = db_factory(init=init_script)

test_script = """
    set heading off;
    select v.rdb$db_key, v.*
    from v;
"""

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    
    failed_chk = []
    for i, line in enumerate(act.clean_stdout.splitlines()):
        if (s := line.strip()):
            f01, f02, f03 = s.split()[:3]
            k02 = '0' * 16 if f02 == '<null>'else f02
            k03 = '0' * 16 if f03 == '<null>'else f03
            if f01 == k03 + k02:
                pass
            else:
                failed_chk.append( f"line {i:3d}: FAILED. {f01} <> {k03} || {k02}" )

    act.reset()

    # failed_chk must contain only lines which did NOT pass check:
    for x in failed_chk:
        print(x)

    act.expected_stdout = """
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
