#coding:utf-8

"""
ID:          issue-8168
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8168
TITLE:       MAKE_DBKEY bug after backup/restore
DESCRIPTION:
    Test creates two tables (tab_1, tab_2) and stores their relation_id in appropriate context variables.
    Then we drop these tables and create them again but in 'reverse' order: tab_2, tab_1.
    At this point we have to check that values of relation_id must differ for both tables and raise error
    (see 'exc_rel_id_not_changed') if this is not so. Test logic must be changed if this error raises.
    Then we add one row into each table and create SP (sp_chk) that uses make_dbkey() for returning these
    rows. Key note: this SP *must* find appropriate record for each table.
    Otherwise (if record not found) we raise exception exc_invalid_make_dbkey.
    Finally, we do backup / restore and repeat call of sp_chk. It must return same ID values as before b/r.
NOTES:
    [16.07.2024] pzotov
    Confirmed bug on 6.0.0.386, 5.0.1.1425.
    Checked on 6.0.0.387, 5.0.1.1428.

    Thanks to Vlad for suggestion about test implementation.
"""
import pytest
from firebird.qa import *
from io import BytesIO
from firebird.driver import SrvRestoreFlag
import locale

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, capsys):

    init_sql = f"""
        set bail on;
        set list on;
        create exception exc_rel_id_not_changed 'RELATION_ID not changed table(s): @1. One need to change test logic!';
        create exception exc_invalid_make_dbkey 'Invalid make_dbkey() detected for table(s): @1';

        create view v_get_rel_id as
        select
            max( iif( upper(rdb$relation_name) = 'TAB_1', rdb$relation_id, null ) ) as t1_rel_id
           ,max( iif( upper(rdb$relation_name) = 'TAB_2', rdb$relation_id, null ) ) as t2_rel_id
        from rdb$relations
        where rdb$relation_name starting with upper('tab_')
        ;
        create table tab_1(id int);
        create table tab_2(id int);
        commit;

        set term ^;
        execute block as
        begin
            for
                select t1_rel_id, t2_rel_id
                from v_get_rel_id
                as cursor c
            do begin
                rdb$set_context('USER_SESSION', 'TAB1_REL_ID', c.t1_rel_id);
                rdb$set_context('USER_SESSION', 'TAB2_REL_ID', c.t2_rel_id);
            end
        end
        ^
        set term ;^

        drop table tab_1;
        drop table tab_2;

        recreate table tab_2(id int);
        recreate table tab_1(id int);
        commit;

        set term ^;
        execute block as
            declare v_list varchar(100) = '';
        begin
            for
                select t1_rel_id, t2_rel_id
                from v_get_rel_id
                as cursor c
            do begin
                if (c.t1_rel_id = rdb$get_context('USER_SESSION', 'TAB1_REL_ID')) then
                    v_list = v_list || 'TAB_1; ';
                if (c.t2_rel_id = rdb$get_context('USER_SESSION', 'TAB2_REL_ID')) then
                    v_list = v_list || 'TAB_2';
                
                if (v_list > '') then
                    exception exc_rel_id_not_changed using(v_list);
            end
        end
        ^
        set term ;^

        -----------------------------------------

        insert into tab_1(id) values(1);
        insert into tab_2(id) values(2);
        commit;

        set term ^;
        create or alter procedure sp_chk returns (id1 int, id2 int) as
            declare v_list varchar(100) = '';
        begin
            select id from tab_1 where rdb$db_key = make_dbkey('TAB_1', 0) into id1;
            if (row_count = 0) then v_list = v_list || 'TAB_1; ';

            select id from tab_2 where rdb$db_key = make_dbkey('TAB_2', 0) into id2;
            if (row_count = 0) then v_list = v_list || 'TAB_2';

            if (v_list > '') then
                exception exc_invalid_make_dbkey using(v_list);

            suspend;
        end
        ^
        set term ;^
        commit;
        select * from sp_chk;

    """

    expected_stdout = """
        ID1 1
        ID2 2
    """
    act.expected_stdout = expected_stdout
    act.isql(input = init_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #---------------------------------------------------

    backup = BytesIO()

    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=act.db.db_path, flags = SrvRestoreFlag.REPLACE)

    act.expected_stdout = """
        ID1 1
        ID2 2
    """
    act.isql(input = "set list on; select * from sp_chk;", combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
