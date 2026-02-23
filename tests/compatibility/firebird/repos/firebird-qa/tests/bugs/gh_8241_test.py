#coding:utf-8
"""
ID:          issue-8241
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8241
TITLE:       gbak may lose NULLs in restore
DESCRIPTION: 
NOTES:
    Restore must use gbak utility, target DB must be prefixed by 'localhost:'.

    Confirmed bug on 6.0.0.447, 5.0.2.1487.
    Checked on 6.0.0.450-8591572, 5.0.2.1493-eb720e8.
"""

import pytest
from firebird.qa import *
from pathlib import Path
import time

init_sql = """
    create table t ("TABLE" integer);
    insert into t values (null);
    insert into t values (null);
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db')

tmp_fbk = temp_file('tmp_8241.fbk')
tmp_res = temp_file('tmp_8241.fdb')

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, tmp_fbk: Path, tmp_res: Path, capsys):

    with act.connect_server() as srv:
        srv.database.set_sql_dialect(database=act.db.db_path, dialect=1)

    act.gbak(switches=['-b', act.db.db_path, str(tmp_fbk)])
    act.gbak(switches=['-rep', str(tmp_fbk), f'localhost:{act.db.db_path}'])

    # NOTE! THIS PREVENTS FROM REPRODUCING BUG:
    # DO NOT USE >>> act.gbak(switches=['-se', 'localhost:service_mgr', '-rep', str(tmp_fbk), act.db.db_path])

    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute('select "TABLE" from t')
        for r in cur:
            print(r[0])

    act.expected_stdout = f"""
        None
        None
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
