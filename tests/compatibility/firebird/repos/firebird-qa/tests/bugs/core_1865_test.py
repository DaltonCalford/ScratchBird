#coding:utf-8

"""
ID:          issue-2295
ISSUE:       2295
TITLE:       BLR error on restore database with computed by Field
DESCRIPTION:
JIRA:        CORE-1865
FBTEST:      bugs.core_1865
"""

import pytest
from io import BytesIO
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

init_script = """
create table tmain(id int);
create table tdetl( id int, pid int, cost numeric(12,2) );
alter table tmain
   add dsum2 computed by ( (select sum(cost) from tdetl d where d.pid = tmain.id) ) ;
commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        # test fails if restore raises an exception
        srv.database.local_restore(backup_stream=backup, database=act.db.db_path,
                                   flags=SrvRestoreFlag.ONE_AT_A_TIME | SrvRestoreFlag.REPLACE)

