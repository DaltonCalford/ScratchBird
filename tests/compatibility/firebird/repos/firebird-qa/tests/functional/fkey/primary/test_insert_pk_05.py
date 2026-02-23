#coding:utf-8

"""
ID:          fkey.primary.insert-05
FBTEST:      functional.fkey.primary.insert_pk_05
TITLE:       Detail Tx should be able to insert data that matches to newly added PK if master Tx has committed.
DESCRIPTION:
    Check foreign key work.
    Master transaction modifies primary key and committed
    Detail transaction inserts record in detail_table.
    Expected: no errors
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, DatabaseError

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
        cust_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
        con.begin(cust_tpb)
        with con.cursor() as c:
            c.execute("UPDATE MASTER_TABLE SET ID=2 WHERE ID=1")
            con.commit()

            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cur_detl:
                    try:
                        cur_detl.execute('insert into detail_table (id, fkey) values (1,2)')
                    except DatabaseError as e:
                        print(e.__str__())
                        print(e.gds_codes)

    # No output must be here.
    act.expected_stdout = f"""
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
