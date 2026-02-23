#coding:utf-8

"""
ID:          issue-4621
ISSUE:       4621
TITLE:       fbsvcmgr doesn't recognise sts_record_versions and other sts switches
DESCRIPTION:
JIRA:        CORE-4298
FBTEST:      bugs.core_4298
"""

import pytest
from firebird.qa import *
from firebird.driver import SrvStatFlag

init_script = """
    recreate table test(id int, x int);
    commit;
    insert into test values(1, 100);
    insert into test values(2, 200);
    insert into test values(3, 300);
    insert into test values(4, 400);
    insert into test values(5, 500);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('Average version length: [\\d]+.[\\d]+, total versions: 5, max versions: 1',
                                         'total versions: 5, max versions: 1')])

expected_stdout = """
    Average version length: 9.00, total versions: 5, max versions: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('update test set x = -x')
        con.commit()
    act.svcmgr(switches=['action_db_stats', 'dbname',
                           str(act.db.db_path), 'sts_record_versions'])
    act.stdout = '\n'.join([line for line in act.stdout.splitlines() if 'versions:' in line.lower()])
    act.expected_stdout = expected_stdout
    assert act.clean_stdout == act.clean_expected_stdout
