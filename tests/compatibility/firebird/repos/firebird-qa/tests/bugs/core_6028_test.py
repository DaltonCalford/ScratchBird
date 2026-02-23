#coding:utf-8

"""
ID:          issue-6278
ISSUE:       6278
TITLE:       Trigger on system table restored in FB3 database and can't be deleted
DESCRIPTION:
  We restore here database that was created in FB 2.5.9 and contained triggers for tables RDB$RELATION_FIELDS, MON$STATEMENTS and MON$ATTACHMENTS.
  Table RDB$RELATION_FIELDS had triggers BEFORE INSERT and AFTER INSERT. Monitoring tabled had triggers BEFORE DELETE and AFTER DELETE.
  Also, table 'TLOG' is in this database, and this table serves as log for actions: create/drop table; delete from mon$statements and delete from mon$attachments.
  For DDL like 'create table test(x int)' and 'drop table test' table TLOG will contain two records which are added there by triggers on RDB$RELATION_FIELDS.
  Further, if we create addition connection and run statement which returns at least one record (like 'select ... from rdb$database') then in 2.5 two recors
  had been added into TLOG for each of: 'DELETE FROM MON$STATEMENTS' and 'DELETE FROM MON$ATTACHMENTS'.

  Finally, BEFORE fix of this ticket issue (e.g. in WI-V3.0.5.33109):
  1) restored database contained following triggers: TRG_MON_ATTACHMENTS*, TRG_MON_STATEMENTS* and TRG_RDB_REL_FIELDS*
  2) statements 'create table' and 'drop table' led to logging following records in TLOG:
      rdb$relation_fields: record is to be created
      rdb$relation_fields: record has been created
      rdb$relation_fields: record is to be removed
      rdb$relation_fields: record has been removed
  3) command 'delete from mon$statement' (when there was another non-system connection with one running or completed statement)
     led to logging these records in TLOG:
       mon$statements: record is to be removed
       mon$statements: record has been removed
  4) command 'delete from mon$attachments' (when there was another non-system connection) led to logging these records in TLOG:
      mon$attachments: record is to be removed
      mon$attachments: record has been removed

  All of above mentioned should NOT appear in a database that is restored AFTER this ticket was fixed.
  Finally, we try to create three new triggers for tables rdb$relation-fields, mon$statements and mon$attachments.
  All of these attempts must FAIL with:
  ========
    - no permission for ALTER access to TABLE RDB$RELATION_FIELDS
    -607
    335544351
  ========
JIRA:        CORE-6028
FBTEST:      bugs.core_6028
NOTES:
    [18.07.2025] pzotov
    1. Regression did exist after SQL schemas introduction (letter to Alex and Adriano, 03.07.2025),
       see also: https://github.com/FirebirdSQL/firebird/issues/6278#issuecomment-3033249058
    2. Separated expected output for FB major versions prior/since 6.x.
       No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.1020; 5.0.3.1683; 4.0.6.3221; 3.0.13.33813
"""

import pytest
import zipfile
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag, DatabaseError

db = db_factory()
db_tmp = db_factory(filename='tmp_core_602.fdb', do_not_create=True)

act = python_act('db')

fbk_file = temp_file('core_6028_25.fbk')

ddl_probes = ( 
    """
        create or alter trigger new_trg_rdb_rel_flds_bi for rdb$relation_fields active before insert position 0 as
        begin
           insert into tlog(id, action) values( gen_id(g, 111), 'rdb$relation_fields: record is to be created' );
        end
    """
    ,
    """
        create or alter trigger new_trg_mon_stm_bd for mon$statements active before delete position 0 as
        begin
           insert into tlog(id, action) values( gen_id(g, 222), 'mon$statements: record is to be removed' );
        end
    """
    ,
    """
        create or alter trigger new_trg_mon_att_bd for mon$attachments active before delete position 0 as
        begin
           insert into tlog(id, action) values( gen_id(g, 333), 'mon$attachments: record is to be removed' );
        end
    """
)

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, fbk_file: Path, db_tmp: Database, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'core_6028_25.zip', at='core_6028_25.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())
    #
    with act.connect_server() as srv:
        srv.database.restore(backup=fbk_file, database=db_tmp.db_path,
                             flags=SrvRestoreFlag.REPLACE)
        srv.wait()
    #
    con_worker = db_tmp.connect()
    con_worker_attachment_id = con_worker.info.id
    con_worker.execute_immediate('create table test(id int)')
    con_worker.commit()
    con_worker.execute_immediate('drop table test')
    con_worker.commit()
    #
    cur_worker=con_worker.cursor()
    cur_worker.execute("select coalesce(rt.rdb$trigger_name, 'NO USER-DEFINED TRIGGERS IN JUST RESTORED DATABASE.') from rdb$database rd left join rdb$triggers rt on rt.rdb$system_flag is distinct from 1 order by 1")
    for r in cur_worker:
        print(r[0])
    #
    with db_tmp.connect() as con_killer:
        cur_killer = con_killer.cursor()
        cur_killer.execute(f'delete from mon$statements s where s.mon$attachment_id = {con_worker_attachment_id}')
        con_killer.commit()
        cur_killer.execute(f'delete from mon$attachments a where a.mon$attachment_id = {con_worker_attachment_id}')
        con_killer.commit()
        cur_killer.execute("select coalesce(t.action, 'NO ACTIONS WAS LOGGED IN THE TABLE TLOG.') as sys_tabs_action from rdb$database rd left join tlog t on 1=1")
        for r in cur_killer:
            print(r[0])
        #
        try:
            cur_worker.close()
        except Exception:
            pass
        try:
            con_worker.close()
        except Exception:
            pass
        #
        for cmd in ddl_probes:
            try:
                con_killer.execute_immediate(cmd)
            except DatabaseError as e:
                print(e)
                print(e.sqlcode)
                print(e.gds_codes)
    act.reset()

    expected_stdout_5x = """
        NO USER-DEFINED TRIGGERS IN JUST RESTORED DATABASE.
        NO ACTIONS WAS LOGGED IN THE TABLE TLOG.
        unsuccessful metadata update
        -CREATE OR ALTER TRIGGER NEW_TRG_RDB_REL_FLDS_BI failed
        -no permission for ALTER access to TABLE RDB$RELATION_FIELDS
        -607
        (335544351, 336397272, 335544352)
        unsuccessful metadata update
        -CREATE OR ALTER TRIGGER NEW_TRG_MON_STM_BD failed
        -no permission for ALTER access to TABLE MON$STATEMENTS
        -607
        (335544351, 336397272, 335544352)
        unsuccessful metadata update
        -CREATE OR ALTER TRIGGER NEW_TRG_MON_ATT_BD failed
        -no permission for ALTER access to TABLE MON$ATTACHMENTS
        -607
        (335544351, 336397272, 335544352)
    """

    expected_stdout_6x = """
        NO USER-DEFINED TRIGGERS IN JUST RESTORED DATABASE.
        NO ACTIONS WAS LOGGED IN THE TABLE TLOG.
        CREATE OR ALTER TRIGGER "SYSTEM"."NEW_TRG_RDB_REL_FLDS_BI" failed
        -Cannot CREATE/ALTER/DROP TRIGGER in SYSTEM schema
        -901
        (336397272, 336068927)
        CREATE OR ALTER TRIGGER "SYSTEM"."NEW_TRG_MON_STM_BD" failed
        -Cannot CREATE/ALTER/DROP TRIGGER in SYSTEM schema
        -901
        (336397272, 336068927)
        CREATE OR ALTER TRIGGER "SYSTEM"."NEW_TRG_MON_ATT_BD" failed
        -Cannot CREATE/ALTER/DROP TRIGGER in SYSTEM schema
        -901
        (336397272, 336068927)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
