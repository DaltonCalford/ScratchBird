#coding:utf-8

"""
ID:          fkey.primary.insert-04
FBTEST:      functional.fkey.primary.insert_pk_04
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
    Check foreign key work.
    Master transaction:
        1) modifies primary key
        2) create savepoint
        3) modifies non key field
        4) rollback to savepoint
    Detail transaction inserts record in detail_table.
    Expected: error because key field in master_table was changed
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, tpb, Isolation

init_script = """
    create table master_table (
        id     integer primary key,
        int_f  integer
    );

    create table detail_table (
        id    integer primary key,
        fkey  integer
    );

    alter table detail_table add constraint fk_detail_table foreign key (fkey) references master_table (id);
    commit;
    insert into master_table (id, int_f) values (1, 10);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        custom_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
        con.begin(custom_tpb)
        with con.cursor() as cur_main:
            cur_main.execute('UPDATE MASTER_TABLE SET ID=2 WHERE ID=1')
            con.savepoint('A')
            cur_main.execute('UPDATE MASTER_TABLE SET INT_F=2')
            con.rollback(savepoint='A')

            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(custom_tpb)
                with con_detail.cursor() as cur_detl:
                    try:
                        cur_detl.execute('insert into detail_table (id, fkey) values (1,1)')
                    except DatabaseError as e:
                        print(e.__str__())
                        print(e.gds_codes)

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    act.expected_stdout = f"""
        violation of FOREIGN KEY constraint "FK_DETAIL_TABLE" on table {SQL_SCHEMA_PREFIX}"DETAIL_TABLE"
        -Foreign key reference target does not exist
        -Problematic key value is ("FKEY" = 1)
        (335544466, 335544838, 335545072)
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

