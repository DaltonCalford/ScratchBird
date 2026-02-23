#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8488
TITLE:       MIN/MAX aggregates may badly affect the join order in queries with mixed INNER/LEFT joins
DESCRIPTION:
NOTES:
    [31.01.2026] pzotov
    No demo database has been provided in the ticket so i've tried to reproduce problem based on my guesses.
    Commits that did fix:
        5.x: afab69577209a82acf63e48779370caf806e9202 -- 31.03.25 17:02 UTC;
        6.x: 45f87c2025cfb86fd217a6902ef34f2f57b26262 -- 01.04.25 09:37 UTC.

    This fix caused to changing of execution plan if we use aggregate function MIN() but not if COUNT().
    Also, number of fetches significantly reduced on snapshots since fix (at least on example provided here).

    Improvement can be seen in comparison of snapshots:
        5.0.3.1638-6579c7d: 2061416 fetches // last snapshot before fix (28-03-25 20:18 UTC)
        5.0.3.1639-67152c7: 1172507 fetches // first snapshot after fix (31-03-25 20:18 UTC)
    Sent report to dimitr, 30.01.2026 21:19.

    Checked on 6.0.0.1400; 5.0.4.1748.
"""

from firebird.driver import DatabaseError
import pytest
from firebird.qa import *

db = db_factory()
act = isql_act('db')

#-----------------------------------------------------------
def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped
#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    init_script = """
        set bail on;
        recreate sequence g;
        recreate table inv_card(id int not null, usr int);
        recreate table balance(
            id int not null
           ,card_key int     -- FK to inv_card
           ,good_key int    -- FK to goods_lst
           ,total int
        );
        recreate table goods_lst(id int not null, good_key int);
        recreate table goods_ref(id int not null);
        commit;

        set term ^;
        execute block as
            declare n_inv int = 10;
            declare n_usr int = 30;
            declare n_gref int = 100;
            declare n_glst int = 10000;
            declare n_bal int = 500000;
        begin
            execute statement 'alter sequence g restart with 0';

            while (n_inv > 0) do
            begin
                insert into inv_card(id, usr) values(gen_id(g,1), mod(gen_id(g,0), :n_usr));
                n_inv = n_inv - 1;
            end

            while (n_gref > 0) do
            begin
                insert into goods_ref(id) values(:n_gref);
                n_gref = n_gref - 1;
            end

            select count(*) from goods_ref into n_gref;

            execute statement 'alter sequence g restart with 0';

            while (n_glst > 0) do
            begin
                insert into goods_lst(id, good_key) values(gen_id(g,1), 1 + mod(:n_glst, :n_gref) );
                n_glst = n_glst - 1;
            end

            select count(*) from goods_ref into n_gref;
            select count(*) from inv_card into n_inv;
            while (n_bal > 0) do
            begin
                insert into balance(id, card_key, good_key, total) values(gen_id(g,1), mod(:n_bal, :n_inv), 1 + mod(:n_glst, :n_gref), rand() );
                n_bal = n_bal - 1;
            end

        end
        ^
        set term ;^
        commit;

        alter table inv_card add constraint inv_pk primary key(id);
        alter table balance add constraint bal_pk primary key(id);
        alter table goods_lst add constraint g_lst_pk primary key(id);
        
        alter table goods_ref add constraint g_ref_pk primary key(id);

        alter table balance add constraint bal_fk_inv foreign key(card_key) references inv_card(id);
        alter table balance add constraint bal_fk_gd foreign key(good_key) references goods_lst(id);
        alter table goods_lst add constraint gd_fk_gr foreign key(good_key) references goods_ref(id);
        create index bal_total_non_zero on balance computed by( total<>0 );
        commit;

    """

    act.isql(switches = ['-q'], input = init_script, combine_output = True)
    assert act.clean_stdout == '', 'Init script FAILED: {act.clean_stdout=}'
    act.reset()

    qry_map = {
        1000 :
        """
            select /* trace_me */ min(gr.id)
            from inv_card c
            left join balance m on c.id = m.card_key
            inner join goods_lst g on (g.id = m.good_key)
            left join goods_ref gr on (g.good_key = gr.id)
            where m.total <> 0
                  and c.usr > 0  
        """
    }


    with act.db.connect() as con:
        cur = con.cursor()
        for k, v in qry_map.items():
            ps, rs = None, None
            try:
                ps = cur.prepare(v)

                print(v)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                print('')
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()
        


    expected_stdout_5x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Nested Loop Join (outer)
        ................-> Filter
        ....................-> Hash Join (inner)
        ........................-> Nested Loop Join (inner)
        ............................-> Filter
        ................................-> Table "INV_CARD" as "C" Full Scan
        ............................-> Filter
        ................................-> Table "BALANCE" as "M" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "BAL_FK_INV" Range Scan (full match)
        ........................-> Record Buffer (record length: 33)
        ............................-> Table "GOODS_LST" as "G" Full Scan
        ................-> Filter
        ....................-> Table "GOODS_REF" as "GR" Access By ID
        ........................-> Bitmap
        ............................-> Index "G_REF_PK" Unique Scan
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Nested Loop Join (outer)
        ................-> Filter
        ....................-> Hash Join (inner) (keys: 1, total key length: 4)
        ........................-> Nested Loop Join (inner)
        ............................-> Filter
        ................................-> Table "PUBLIC"."INV_CARD" as "C" Full Scan
        ............................-> Filter
        ................................-> Table "PUBLIC"."BALANCE" as "M" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PUBLIC"."BAL_FK_INV" Range Scan (full match)
        ........................-> Record Buffer (record length: 33)
        ............................-> Table "PUBLIC"."GOODS_LST" as "G" Full Scan
        ................-> Filter
        ....................-> Table "PUBLIC"."GOODS_REF" as "GR" Access By ID
        ........................-> Bitmap
        ............................-> Index "PUBLIC"."G_REF_PK" Unique Scan
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

