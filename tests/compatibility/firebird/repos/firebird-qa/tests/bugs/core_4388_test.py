#coding:utf-8

"""
ID:          issue-4710
ISSUE:       4710
TITLE:       SELECT WITH LOCK may enter an infinite loop for a single record
DESCRIPTION:
JIRA:        CORE-4388
FBTEST:      bugs.core_4388
"""

import pytest
import subprocess
import time
from pathlib import Path
from firebird.qa import *

init_script = """
    create table test(id int primary key, x int);
    commit;
    insert into test values(1, 100);
    commit;
"""

db = db_factory(sql_dialect=3, init=init_script)

act = python_act('db',
                 substitutions=[('(-)?concurrent\\s+transaction\\s+number(\\s+is)?\\s+\\d+',
                                 'concurrent transaction'),
                                ('After\\s+line\\s+\\d+.*', '')])

expected_stdout = """
    Statement failed, SQLSTATE = 40001
    deadlock
    -update conflicts with concurrent update
    -concurrent transaction number is 13
"""

script_file = temp_file('test-script.sql')
script_out = temp_file('test-script.out')

@pytest.mark.version('>=3.0')
def test_1(act: Action, script_file: Path, script_out: Path):
    with act.db.connect() as att1:
        # Delete record but not yet commit - it's a time
        # to make another connection:
        att1.execute_immediate("delete from test where id = 1")
        script_file.write_text("""
        set list on;
        -- set echo on;
        commit;
        set transaction lock timeout 20;
        select x from test where id = 1 with lock;
        """)
        try:
            with open(script_out, mode='w') as output:
                p_test_sql = subprocess.Popen([act.vars['isql'], '-n', '-i', str(script_file),
                                               '-user', act.db.user,
                                               '-password', act.db.password, act.db.dsn],
                                               stdout=output, stderr=subprocess.STDOUT)
            #
            time.sleep(2)
        finally:
            att1.commit()
            p_test_sql.wait()
        # Check
        act.expected_stdout = expected_stdout
        act.stdout = script_out.read_text()
        assert act.clean_stdout == act.clean_expected_stdout
