#coding:utf-8

"""
ID:          issue-5843
ISSUE:       5843
TITLE:       Bugcheck on queries containing WITH LOCK clause
DESCRIPTION:
  We create database as it was show in the ticket and do backup and restore of it.
  Then we run checking query - launch isql two times and check that 2nd call of ISQL
  does not raise bugcheck. Finally we run online validation against this DB.

  Neither test query nor validation should raise any output in the STDERR.
JIRA:        CORE-5576
FBTEST:      bugs.core_5576
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
      recreate table test (
          i integer not null primary key,
          d char(1024) computed by ('qwert'),
          s varchar(8192)
      );
      insert into test values (1, 'format1opqwertyuiopqwertyuiop');
      commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('[0-9][0-9]:[0-9][0-9]:[0-9][0-9].[0-9][0-9]', ''),
                                      ('Relation [0-9]{3,4}', 'Relation')])

expected_stdout_a = """
    X1                              1
"""

expected_stdout_b_5x = """
    Validation started
    Relation 128 (TEST)
      process pointer page    0 of    1
    Index 1 (RDB$PRIMARY1)
    Relation 128 (TEST) is ok
    Validation finished
"""

expected_stdout_b_6x = """
    Validation started
    Relation ("PUBLIC"."TEST")
    process pointer page    0 of    1
    Index 1 ("PUBLIC"."RDB$PRIMARY1")
    Relation ("PUBLIC"."TEST") is ok
    Validation finished
"""

fbk_file = temp_file('core_5576.fbk')
fdb_file = temp_file('core_5576.fdb')

@pytest.mark.version('>=3.0.3')
def test_1(act: Action, fbk_file: Path, fdb_file: Path):
    act.gbak(switches=['-b', act.db.dsn, str(fbk_file)])
    act.reset()
    act.gbak(switches=['-rep', str(fbk_file), act.get_dsn(fdb_file)])
    #
    for i in range(2): # Run isql twice!
        act.reset()
        act.expected_stdout = expected_stdout_a
        act.isql(switches=[act.get_dsn(fdb_file)], connect_db=False,
                 input='set list on;select 1 x1 from test where i=1 with lock;')
        assert act.clean_stdout == act.clean_expected_stdout
    # Validate the database
    act.reset()
    
    act.expected_stdout = expected_stdout_b_5x if act.is_version('<6') else expected_stdout_b_6x

    with act.connect_server() as srv:
        srv.database.validate(database=fdb_file)
        act.stdout = ''.join(srv.readlines())
    assert act.clean_stdout == act.clean_expected_stdout


