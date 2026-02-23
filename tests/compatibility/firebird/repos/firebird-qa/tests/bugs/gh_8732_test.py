#coding:utf-8

"""
ID:          8732
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8732
TITLE:       Add support of the IN <list> predicate to the equality distribution logic
DESCRIPTION:
NOTES:
    [10.09.2025]
    Confirmed issue on 6.0.0.1266; 5.0.4.1704 ('table "B" full scan' in explained plan).
    Checked on 6.0.0.1273; 5.0.4.1706.
"""

import pytest
from firebird.qa import *

init_script = """
    set list on;
    recreate table test_a(id int primary key using descending index test_a_desc_pk);
    recreate table test_b(id int primary key using index test_b_pk);

    set term ^;
    execute block as
        declare n int = 10000;
    begin
        while (n>0) do
        begin
            insert into test_a(id) values(:n);
            n = n - 1;
        end
    end
    ^
    set term ;^

    insert into test_b(id) select id from test_a order by id rows ( (select count(*) from test_a)/20 );
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('record length.*', ''), ('key length.*', '')])

#-----------------------------------------------------------
def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped
#-----------------------------------------------------------

@pytest.mark.version('>=5.0.4')
def test_1(act: Action, capsys):
    qry_map = {
        1000 :
        """
            select a.id as a_id
            from test_a as a
            join test_b as b on a.id = b.id
            where a.id in (1,2)
            order by a.id desc
        """
        ,
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

                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                #rs = cur.execute(ps)
                #for r in rs:
                #    print(r[0], r[1])
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
        ....-> Sort (
        ........-> Nested Loop Join (inner)
        ............-> Filter
        ................-> Table "TEST_B" as "B" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_B_PK" List Scan (full match)
        ............-> Filter
        ................-> Table "TEST_A" as "A" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_A_DESC_PK" Unique Scan
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Sort (
        ........-> Nested Loop Join (inner)
        ............-> Filter
        ................-> Table "PUBLIC"."TEST_B" as "B" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_B_PK" List Scan (full match)
        ............-> Filter
        ................-> Table "PUBLIC"."TEST_A" as "A" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_A_DESC_PK" Unique Scan
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
