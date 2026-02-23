#coding:utf-8

"""
ID:          fkey.primary.insert-07
FBTEST:      functional.fkey.primary.insert_pk_07
ISSUE:       2027
JIRA:        CORE-1606
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
    Master table has primary key consisting of several fields.
    Master transaction modifies non key fields.
    Detail transaction inserts record in detail_table.
    Expected: no errors.
    Related to #2027. Ability to insert child record if parent record is locked but foreign key target unchanged.
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, DatabaseError

init_script = """
    create table master_table (
        id_1 integer not null,
        id_2 varchar(20) not null,
        non_key_fld  integer,
        primary key (id_1, id_2)
    );

    create table detail_table (
        id    integer primary key,
        fkey_1  integer,
        fkey_2  varchar(20)
    );

    alter table detail_table add constraint fk_detail_table foreign key (fkey_1, fkey_2) references master_table (id_1, id_2);
    commit;
    insert into master_table (id_1, id_2, non_key_fld) values (1, 'one', 10);
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
            cur_main.execute('update master_table set non_key_fld=2')

            with act.db.connect() as con_detail:
                con_detail.begin(custom_tpb)
                with con_detail.cursor() as cur_detl:
                    try:
                        cur_detl.execute("insert into detail_table (id, fkey_1, fkey_2) values (1, 1, 'one')")
                    except DatabaseError as e:
                        print(e.__str__())
                        print(e.gds_codes)

    # No output must be here.
    act.expected_stdout = f"""
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
