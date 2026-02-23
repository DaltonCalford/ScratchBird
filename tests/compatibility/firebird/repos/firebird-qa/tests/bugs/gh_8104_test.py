#coding:utf-8

"""
ID:          issue-8104
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8104
TITLE:       Inefficient evaluation of expressions like rdb$db_key <= ? after mass delete
DESCRIPTION:
    Test does actions described in the ticket but operates with first PP of table instead of 20th
    (see variable 'chk_pp').
    Following query is performed two times (see variable 'read_records_for_chk_pp'):
    =========
    select ...
    from t1
    where
        rdb$db_key >= make_dbkey({rel_id}, 0, 0, {chk_pp})
        and rdb$db_key < make_dbkey({rel_id}, 0, 0, {chk_pp+1})
    =========
    We compare number of fetches in this query before and after bulk deletion ('fetches_1', 'fetches_2').
    Value 'fetches_2' must be LESS than 'fetches_1' (before fix it was much greater).
NOTES:
    [08.05.2024] pzotov
    Confirmed problem on 6.0.0.344, 5.0.1.1394, 4.0.5.3091 (fetches in request #1: 47643; in request #2: 115943).
    Checked on 6.0.0.345, 5.0.1.1395, 4.0.5.3092 (fetches in req #2 LESS than in req #1).
"""

import pytest
from firebird.qa import *

TAB_NAME = 'T1'.upper()
ROWS_CNT = 100000

init_sql = f"""
    create table {TAB_NAME} (
        id int not null,
        val varchar(256)
    );
    commit;

    -- fill with some data
    set term ^;
    execute block as
        declare n int = 0;
        declare s varchar(36);
    begin
        while (n < {ROWS_CNT}) do
        begin
            n = n + 1;
            s = uuid_to_char(gen_uuid());
            insert into {TAB_NAME} (id, val) values (:n, lpad('', 256, :s));
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db')

@pytest.mark.version('>=4.0.5')
def test_1(act: Action, capsys):

    get_last_pp_for_table = f"""
        select p.rdb$relation_id, p.rdb$page_sequence
        from rdb$pages p join rdb$relations r on p.rdb$relation_id = r.rdb$relation_id
        where r.rdb$relation_name = '{TAB_NAME}' and p.rdb$page_type = 4
        order by 2 desc
        rows 1
    """
    rel_id, max_pp = -1, -1
    with act.db.connect(no_gc = True) as con:
        cur = con.cursor()
        cur.execute(get_last_pp_for_table)
        for r in cur:
            rel_id, max_pp = r[:2]
        assert rel_id > 0 and max_pp > 0
        #--------------------------------
        
        # Subsequent number of PP that we want to check ('20' in the ticket):
        ##########
        chk_pp = 0
        ##########

        read_records_for_chk_pp = f"""
            select count(*), min(id), max(id)
            from t1
            where
                rdb$db_key >= make_dbkey({rel_id}, 0, 0, {chk_pp})
                and rdb$db_key < make_dbkey({rel_id}, 0, 0, {chk_pp+1})
        """

        fetches_ini = con.info.fetches
        # read records from selected PP only -- FIRST TIME
        cur.execute(read_records_for_chk_pp)
        cur.fetchall()
        fetches_1 = con.info.fetches - fetches_ini

        #----------------------------------

        # delete records from selected PP and up to the end
        del_rows_starting_from_chk_pp = f"""
            delete from t1
            where rdb$db_key >= make_dbkey({rel_id}, 0, 0, {chk_pp})
        """
        con.execute_immediate(del_rows_starting_from_chk_pp)

        #----------------------------------

        fetches_ini = con.info.fetches
        # read records from selected PP only -- SECOND TIME
        cur.execute(read_records_for_chk_pp)
        cur.fetchall()
        fetches_2 = con.info.fetches - fetches_ini

    expected_msg = 'Fetches ratio expected.'

    if fetches_2 <= fetches_1:
        print(expected_msg)
    else:
        print(f'Fetches ratio between 1st and 2nd requests to PP = {chk_pp} - UNEXPECTED:')
        print('Request #1:', fetches_1)
        print('Request #2:', fetches_2)

    act.expected_stdout = f"""
        {expected_msg}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
