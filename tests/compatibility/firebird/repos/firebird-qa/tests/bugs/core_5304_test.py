#coding:utf-8

"""
ID:          issue-5581
ISSUE:       5581
TITLE:       Regression: Can not restore database with table contains field CHAR(n) and UTF8 character set
DESCRIPTION:
  We make initial DDL and DML using ISQL with connection charset = UTF8, and then run b/r.
  Ouput of restore is filtered so that only lines with 'ERROR' word can be displayed.
  This output should be EMPTY (i.e. no errors should occur).
JIRA:        CORE-5304
FBTEST:      bugs.core_5304
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
    recreate table test(c char(10));
    commit;
    insert into test values(null);
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db')

fbk_file = temp_file('tmp_core_5304.fbk')
fdb_file = temp_file('tmp_core_5304.fdb')

@pytest.mark.version('>=4.0')
def test_1(act: Action, fbk_file: Path, fdb_file: Path):
    with act.connect_server() as srv:
        srv.database.backup(database=act.db.db_path, backup=fbk_file)
        srv.wait()
        srv.database.restore(backup=fbk_file, database=fdb_file)
        srv.wait()


