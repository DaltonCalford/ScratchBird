#coding:utf-8

"""
ID:          issue-6616
ISSUE:       6616
TITLE:       Unable to restore database with tables using GENERATED ALWAYS AS IDENTITY
  columns (ERROR:OVERRIDING SYSTEM VALUE should be used)
DESCRIPTION:
JIRA:        CORE-6377
FBTEST:      bugs.core_6377
"""

import pytest
from io import BytesIO
from pathlib import Path
from firebird.qa import *

init_script = """
    create table identity_always(id bigint generated always as identity constraint pk_identity_always primary key);
    insert into identity_always default values;
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

fdb_file = temp_file('tmp_6377_rest.fdb')

@pytest.mark.version('>=4.0')
def test_1(act: Action, fdb_file: Path):
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=fdb_file)
    # This should pass without error
    act.isql(switches=[act.get_dsn(fdb_file)], connect_db=False,
               input='insert into identity_always default values;')
