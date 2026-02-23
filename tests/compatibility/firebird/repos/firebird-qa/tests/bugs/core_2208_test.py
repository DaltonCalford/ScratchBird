#coding:utf-8

"""
ID:          issue-2636
ISSUE:       2636
TITLE:       New gbak option to ignore specific tables data during the backup
DESCRIPTION:
  We create four tables with ascii and one with non-ascii (cyrillic) names.
  Each table has one row.
  Then we check that one may to:
  1) skip BACKUP data of some tables
  2) skip RESTORE data for same tables.
  All cases are checked by call 'fbsvcmgr ... bkp_skip_data <pattern>',
  where <pattern> string matches several tables (i.e. we use SIMILAR_TO ability).
JIRA:        CORE-2208
FBTEST:      bugs.core_2208
"""

import pytest
from io import BytesIO
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

init_script = """
     recreate table test_01(id char(1));
     recreate table test_02(id char(1));
     recreate table test_0a(id char(1));
     recreate table test_0b(id char(1));

     recreate table "опечатка"(id char(1));
     commit;

     insert into test_01(id) values('1');
     insert into test_02(id) values('2');
     insert into test_0a(id) values('3');
     insert into test_0b(id) values('4');
     insert into "опечатка"(id) values('ы');
     commit;
     --  similar to '(о|а)(п|ч)(е|и)(п|ч)(а|я)(т|д)(к|г)(а|о)';
     recreate view v_check as
        select 'test_01' as msg, t.id
        from rdb$database left join test_01 t on 1=1
        union all
        select 'test_02' as msg, t.id
        from rdb$database left join test_02 t on 1=1
        union all
        select 'test_0a' as msg, t.id
        from rdb$database left join test_0a t on 1=1
        union all
        select 'test_0b' as msg, t.id
        from rdb$database left join test_0b t on 1=1
        union all
        select 'опечатка' as msg, t.id
        from rdb$database left join "опечатка" t on 1=1
      ;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db')

expected_stdout_a = """
MSG                             test_01
ID                              1
MSG                             test_02
ID                              2
MSG                             test_0a
ID                              <null>
MSG                             test_0b
ID                              <null>
MSG                             опечатка
ID                              ы
Records affected: 5
"""

expected_stdout_b = """
MSG                             test_01
ID                              <null>
MSG                             test_02
ID                              <null>
MSG                             test_0a
ID                              3
MSG                             test_0b
ID                              4
MSG                             опечатка
ID                              ы
Records affected: 5
"""

expected_stdout_c = """
MSG                             test_01
ID                              1
MSG                             test_02
ID                              2
MSG                             test_0a
ID                              3
MSG                             test_0b
ID                              4
MSG                             опечатка
ID                              <null>
Records affected: 5
"""

# We need additional test database for restore
tmp_db = temp_file('extra-test.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_db: Path):
    check_script = 'set list on; set count on; select * from v_check;'
    if tmp_db.is_file():
        tmp_db.unlink()
    with act.connect_server() as srv:
        backup = BytesIO()
        # Run-1: try to skip BACKUP of data for tables 'test_0a' and 'test_0b'.
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup,
                                  skip_data='test_0[[:alpha:]]')
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=tmp_db)
        # check
        act.expected_stdout = expected_stdout_a
        act.isql(switches=[str(tmp_db)], input=check_script, connect_db=False)
        assert act.clean_stdout == act.clean_expected_stdout
        # Run-2: try to skip RESTORE of data for tables 'test_01' and 'test_02'.
        if tmp_db.is_file():
            tmp_db.unlink()
        backup.close()
        backup = BytesIO()
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=tmp_db,
                                   skip_data='test_0[[:digit:]]')
        # check
        act.reset()
        act.expected_stdout = expected_stdout_b
        act.isql(switches=[str(tmp_db)], input=check_script, connect_db=False)
        assert act.clean_stdout == act.clean_expected_stdout
        # Run-3: try to skip BACKUP of data for table "опечатка".
        srv.encoding = 'utf8'
        if tmp_db.is_file():
            tmp_db.unlink()
        backup.close()
        backup = BytesIO()
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup,
                                  skip_data='(о|а)(п|ч)(е|и)(п|ч)(а|я)(т|д)(к|г)(о|а)')
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=tmp_db)
        # check
        act.reset()
        act.expected_stdout = expected_stdout_c
        act.isql(switches=[str(tmp_db)], input=check_script, connect_db=False)
