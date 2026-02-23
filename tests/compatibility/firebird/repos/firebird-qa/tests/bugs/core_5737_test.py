#coding:utf-8

"""
ID:          issue-6001
ISSUE:       6001
TITLE:       Invalid parameters of gds transaction in ISQL
DESCRIPTION:
  ISQL hangs when trying to show various system objects in a case when other attachment has uncommitted changes to that objects
  We create (in Python connection) one table TEST1 with PK and commit transaction.
  Then we create second (similar) table TEST2 but do not commit transaction.
  After this we launch ISQL in async. mode and ask him to perform SHOW TABLE and SHOW INDEX commands.
  ISQL:
  1) should NOT hang (it did this because of launching Tx in read committed NO record_version);
  2) should output only info about table TEST1 and ints PK index.
  3) should not output any info about non-committed DDL of table TEST2.

  Confirmed bug on 3.0.3.32837 and 4.0.0.800 (ISQL did hang when issued any of 'SHOW TABLE' / 'SHOW INDEX' copmmand).
JIRA:        CORE-5737
FBTEST:      bugs.core_5737
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
import subprocess
import time
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

show_script = temp_file('show_script.sql')
show_output = temp_file('show_script.out')

@pytest.mark.version('>=3.0.3')
def test_1(act: Action, show_script: Path, show_output: Path):
    show_script.write_text('show table;show index;')
    with act.db.connect() as con:
        con.execute_immediate('recreate table test1(id int primary key using descending index test1_id_pk_desc)')
        con.commit()
        c = con.cursor()
        c.execute('recreate table test2(id int primary key using descending index test2_id_pk_desc)')
        # WARNING: we launch ISQL here in async mode in order to have ability to kill its
        # process if it will hang!
        with open(show_output, mode='w') as show_out:
            p_show_sql = subprocess.Popen([act.vars['isql'], '-i', str(show_script),
                                           '-user', act.db.user,
                                           '-password', act.db.password, act.db.dsn],
                                          stdout=show_out, stderr=subprocess.STDOUT)
            try:
                time.sleep(4)
            finally:
                p_show_sql.terminate()

    expected_stdout_5x = """
        TEST1
        TEST1_ID_PK_DESC UNIQUE DESCENDING INDEX ON TEST1(ID)
    """

    expected_stdout_6x = """
        PUBLIC.TEST1
        PUBLIC.TEST1_ID_PK_DESC UNIQUE DESCENDING INDEX ON TEST1(ID)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = show_output.read_text()
    assert act.clean_stdout == act.clean_expected_stdout
