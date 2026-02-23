#coding:utf-8

"""
ID:          issue-2625
ISSUE:       2625
TITLE:       Add support for -nodbtriggers switch in gbak into services API
DESCRIPTION:
  We add two database triggers (on connect and on disconnect) and make them do real work only when
  new attachment will be established (see trick with rdb$get_context('USER_SESSION', 'INIT_STATE') ).
  After finish backup we restore database and check that there is no records in 'log' table.
  (if option 'bkp_no_triggers' will be omitted then two records will be in that table).
JIRA:        CORE-2197
FBTEST:      bugs.core_2197
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *
from io import BytesIO
from firebird.driver import SrvRestoreFlag, SrvBackupFlag

init_script = """
    recreate table log( att bigint default current_connection, event_name varchar(20) );
    create sequence g;
    set term ^;
    create or alter trigger trg_attach inactive on connect as
    begin
        if ( rdb$get_context('USER_SESSION', 'INIT_STATE') is null ) then
            insert into log(event_name) values ('attach');
    end
    ^
    create or alter trigger trg_detach inactive  on disconnect as
    begin
        if ( rdb$get_context('USER_SESSION', 'INIT_STATE') is null ) then
            insert into log(event_name) values ('detach');
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    delete from log;
    commit;
    set term ^;
    execute block as begin
       rdb$set_context('USER_SESSION', 'INIT_STATE','1');
    end
    ^
    set term ;^
    alter trigger trg_attach active;
    alter trigger trg_detach active;
    commit;
    --set count on;
    --select * from log;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    check_sql = 'set list on; set count on; select 1, g.* from log g;'
    act.execute()
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup,
                                  flags=SrvBackupFlag.NO_TRIGGERS)
        backup.seek(0)
        act.reset()
        act.expected_stdout = expected_stdout
        act.isql(switches=['-nod'], input=check_sql)
        assert act.clean_stdout == act.clean_expected_stdout
        srv.database.local_restore(backup_stream=backup, database=act.db.db_path,
                                   flags=SrvRestoreFlag.REPLACE)
        backup.close()
        act.reset()
        act.expected_stdout = expected_stdout
        act.isql(switches=['-nod'], input=check_sql)
        assert act.clean_stdout == act.clean_expected_stdout



