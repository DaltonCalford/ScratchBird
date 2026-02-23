#coding:utf-8

"""
ID:          issue-6507
ISSUE:       6507
TITLE:       mapping rules destroyed by backup / restore
DESCRIPTION:
JIRA:        CORE-6265
FBTEST:      bugs.core_6265
"""

import pytest
from io import BytesIO
from pathlib import Path
from firebird.qa import *

db = db_factory()

boss_role = role_factory('db', name='boss')

act = python_act('db')

expected_stdout = """
    RDB$MAP_NAME                    MAP_BOSS
    RDB$MAP_USING                   P
    RDB$MAP_PLUGIN                  WIN_SSPI
    RDB$MAP_DB                      <null>
    RDB$MAP_FROM_TYPE               USER
    RDB$MAP_FROM                    BILL
    RDB$MAP_TO_TYPE                 1
    RDB$MAP_TO                      BOSS
    Records affected: 1
"""

init_ddl = """
    --create role boss;
    create mapping map_boss using plugin win_sspi from user Bill to role boss;
    commit;
    create view v_map as
    select
        rdb$map_name,
        rdb$map_using,
        rdb$map_plugin,
        rdb$map_db,
        rdb$map_from_type,
        rdb$map_from,
        rdb$map_to_type,
        rdb$map_to
    from rdb$auth_mapping;
    commit;
    set list on;
    set count on;
    select * from v_map;
"""

restored_db_1 = temp_file('tmp_6265_1.fdb')
restored_db_2 = temp_file('tmp_6265_2.fdb')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, boss_role: Role, restored_db_1: Path, restored_db_2: Path):
    act.isql(switches=['-q'], input=init_ddl)
    #
    backup = BytesIO()
    with act.connect_server() as srv:
        # BACKUP-RESTORE #1:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(database=restored_db_1, backup_stream=backup)
        # BACKUP-RESTORE #2:
        backup.seek(0)
        backup.truncate()
        srv.database.local_backup(database=restored_db_1, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(database=restored_db_2, backup_stream=backup)
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.isql(switches=[act.get_dsn(restored_db_2)], connect_db=False,
             input='set list on; set count on; select * from v_map;')
    assert act.clean_stdout == act.clean_expected_stdout
