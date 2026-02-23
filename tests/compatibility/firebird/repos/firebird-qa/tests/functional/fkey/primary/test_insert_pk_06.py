#coding:utf-8

"""
ID:          fkey.primary.insert-06
FBTEST:      functional.fkey.primary.insert_pk_06
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
    Check foreign key work.
    Master connecttion subsequently does following:
      * starts Tx-1 and modifies primary key;
      * commits Tx-1;
      * starts Tx-2 and modifies NON-key field;
    Detail transaction inserts record in detail_table.
    Expected: no errors
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, DatabaseError

init_script = """
    create table master_table (
        id     integer primary key,
        non_key_fld  integer
    );

    create table detail_table (
        id    integer primary key,
        fkey  integer
    );

    alter table detail_table add constraint fk_detail_table foreign key (fkey) references master_table (id);
    commit;
    insert into master_table (id, non_key_fld) values (1, 10);
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
            cur_main.execute("update master_table set id=2 where id=1")
            con.commit()
            con.begin(custom_tpb)
            cur_main.execute("update master_table set non_key_fld=10")

            with act.db.connect() as con_detail:
                con_detail.begin(custom_tpb)
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
