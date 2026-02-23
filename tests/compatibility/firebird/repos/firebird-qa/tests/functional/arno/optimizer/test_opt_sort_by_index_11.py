#coding:utf-8

"""
ID:          optimizer.sort-by-index-11
TITLE:       ORDER BY ASC using index (multi)
DESCRIPTION:
  ORDER BY X, Y
  When more fields are given in ORDER BY clause try to use a compound index.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_11
NOTES:
    [08.07.2025] pzotov
    Refactored: explained plan is used to be checked in expected_out.
    Added ability to use several queries and their datasets for check - see 'qry_list' and 'qry_data' tuples.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.930; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

from firebird.driver import DatabaseError
import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_fill_data as begin end;
    recreate table test (
        id1 int,
        id2 int
    );

    set term ^;
    create or alter procedure sp_fill_data
    as
        declare x int;
        declare y int;
    begin
        x = 1;
        while (x <= 50) do
        begin
            y = (x / 10) * 10;
            insert into test(id1, id2) values(:y, :x - :y);
            x = x + 1;
        end
        insert into test (id1, id2) values (0, null);
        insert into test (id1, id2) values (null, 0);
        insert into test (id1, id2) values (null, null);
    end
    ^
    set term ;^
    commit;

    execute procedure sp_fill_data;
    commit;

    create asc  index test_id1_asc on test (id1);
    create desc index test_id1_des on test (id1);

    create asc  index test_id2_asc on test (id2);
    create desc index test_id2_des on test (id2);

    create asc  index test_id1_id2_asc on test (id1, id2);
    create desc index test_id1_id2_des on test (id1, id2);

    create asc  index test_id2_id1_asc on test (id2, id1);
    create desc index test_id2_id1_des on test (id2, id1);
    commit;
"""
db = db_factory(init = init_script)


qry_list = (
    """
    select 'point-01' as msg, t.id2, t.id1
    from test t
    where t.id1 = 30 and t.id2 >= 5
    order by t.id2 asc, t.id1 asc
    """
    ,
    """
    select 'point-02' as msg, t.id2, t.id1
    from test t
    where t.id1 = 30 and t.id2 <= 5
    order by t.id2 desc, t.id1 desc
    """
    ,
    """
    select 'point-03' as msg, t.id2, t.id1
    from test t
    where t.id1 >= 30 and t.id2 = 5
    order by t.id2 asc, t.id1 asc
    """
    ,
    """
    select 'point-04' as msg, t.id2, t.id1
    from test t
    where t.id1 <= 30 and t.id2 = 5
    order by t.id2 desc, t.id1 desc
    """
    ,
    """
    select 'point-05' as msg, t.id2, t.id1
    from test t
    where t.id1 <= 30 and t.id2 <= 5
    order by t.id2 asc, t.id1 asc
    """
    ,
    """
    select 'point-06' as msg, t.id2, t.id1
    from test t
    where t.id1 <= 30 and t.id2 <= 5
    order by t.id2 desc, t.id1 desc
    """
    ,
    """
    select 'point-07' as msg, t.id2, t.id1
    from test t
    where t.id1 >= 30 and t.id2 >= 5
    order by t.id2 asc, t.id1 asc
    """
    ,
    """
    select 'point-08' as msg, t.id2, t.id1
    from test t
    where t.id1 >= 30 and t.id2 >= 5
    order by t.id2 desc, t.id1 desc
    """
    ,
)

data_list = (
    """
        MSG : point-01
        ID2 : 5
        ID1 : 30
        MSG : point-01
        ID2 : 6
        ID1 : 30
        MSG : point-01
        ID2 : 7
        ID1 : 30
        MSG : point-01
        ID2 : 8
        ID1 : 30
        MSG : point-01
        ID2 : 9
        ID1 : 30
    """
    ,
    """
        MSG : point-02
        ID2 : 5
        ID1 : 30
        MSG : point-02
        ID2 : 4
        ID1 : 30
        MSG : point-02
        ID2 : 3
        ID1 : 30
        MSG : point-02
        ID2 : 2
        ID1 : 30
        MSG : point-02
        ID2 : 1
        ID1 : 30
        MSG : point-02
        ID2 : 0
        ID1 : 30
    """
    ,
    """
        MSG : point-03
        ID2 : 5
        ID1 : 30
        MSG : point-03
        ID2 : 5
        ID1 : 40
    """
    ,
    """
        MSG : point-04
        ID2 : 5
        ID1 : 30
        MSG : point-04
        ID2 : 5
        ID1 : 20
        MSG : point-04
        ID2 : 5
        ID1 : 10
        MSG : point-04
        ID2 : 5
        ID1 : 0
    """
    ,
    """
        MSG : point-05
        ID2 : 0
        ID1 : 10
        MSG : point-05
        ID2 : 0
        ID1 : 20
        MSG : point-05
        ID2 : 0
        ID1 : 30
        MSG : point-05
        ID2 : 1
        ID1 : 0
        MSG : point-05
        ID2 : 1
        ID1 : 10
        MSG : point-05
        ID2 : 1
        ID1 : 20
        MSG : point-05
        ID2 : 1
        ID1 : 30
        MSG : point-05
        ID2 : 2
        ID1 : 0
        MSG : point-05
        ID2 : 2
        ID1 : 10
        MSG : point-05
        ID2 : 2
        ID1 : 20
        MSG : point-05
        ID2 : 2
        ID1 : 30
        MSG : point-05
        ID2 : 3
        ID1 : 0
        MSG : point-05
        ID2 : 3
        ID1 : 10
        MSG : point-05
        ID2 : 3
        ID1 : 20
        MSG : point-05
        ID2 : 3
        ID1 : 30
        MSG : point-05
        ID2 : 4
        ID1 : 0
        MSG : point-05
        ID2 : 4
        ID1 : 10
        MSG : point-05
        ID2 : 4
        ID1 : 20
        MSG : point-05
        ID2 : 4
        ID1 : 30
        MSG : point-05
        ID2 : 5
        ID1 : 0
        MSG : point-05
        ID2 : 5
        ID1 : 10
        MSG : point-05
        ID2 : 5
        ID1 : 20
        MSG : point-05
        ID2 : 5
        ID1 : 30
    """
    ,
    """
        MSG : point-06
        ID2 : 5
        ID1 : 30
        MSG : point-06
        ID2 : 5
        ID1 : 20
        MSG : point-06
        ID2 : 5
        ID1 : 10
        MSG : point-06
        ID2 : 5
        ID1 : 0
        MSG : point-06
        ID2 : 4
        ID1 : 30
        MSG : point-06
        ID2 : 4
        ID1 : 20
        MSG : point-06
        ID2 : 4
        ID1 : 10
        MSG : point-06
        ID2 : 4
        ID1 : 0
        MSG : point-06
        ID2 : 3
        ID1 : 30
        MSG : point-06
        ID2 : 3
        ID1 : 20
        MSG : point-06
        ID2 : 3
        ID1 : 10
        MSG : point-06
        ID2 : 3
        ID1 : 0
        MSG : point-06
        ID2 : 2
        ID1 : 30
        MSG : point-06
        ID2 : 2
        ID1 : 20
        MSG : point-06
        ID2 : 2
        ID1 : 10
        MSG : point-06
        ID2 : 2
        ID1 : 0
        MSG : point-06
        ID2 : 1
        ID1 : 30
        MSG : point-06
        ID2 : 1
        ID1 : 20
        MSG : point-06
        ID2 : 1
        ID1 : 10
        MSG : point-06
        ID2 : 1
        ID1 : 0
        MSG : point-06
        ID2 : 0
        ID1 : 30
        MSG : point-06
        ID2 : 0
        ID1 : 20
        MSG : point-06
        ID2 : 0
        ID1 : 10
    """
    ,
    """
        MSG : point-07
        ID2 : 5
        ID1 : 30
        MSG : point-07
        ID2 : 5
        ID1 : 40
        MSG : point-07
        ID2 : 6
        ID1 : 30
        MSG : point-07
        ID2 : 6
        ID1 : 40
        MSG : point-07
        ID2 : 7
        ID1 : 30
        MSG : point-07
        ID2 : 7
        ID1 : 40
        MSG : point-07
        ID2 : 8
        ID1 : 30
        MSG : point-07
        ID2 : 8
        ID1 : 40
        MSG : point-07
        ID2 : 9
        ID1 : 30
        MSG : point-07
        ID2 : 9
        ID1 : 40
    """
    ,
    """
        MSG : point-08
        ID2 : 9
        ID1 : 40
        MSG : point-08
        ID2 : 9
        ID1 : 30
        MSG : point-08
        ID2 : 8
        ID1 : 40
        MSG : point-08
        ID2 : 8
        ID1 : 30
        MSG : point-08
        ID2 : 7
        ID1 : 40
        MSG : point-08
        ID2 : 7
        ID1 : 30
        MSG : point-08
        ID2 : 6
        ID1 : 40
        MSG : point-08
        ID2 : 6
        ID1 : 30
        MSG : point-08
        ID2 : 5
        ID1 : 40
        MSG : point-08
        ID2 : 5
        ID1 : 30
    """
    ,
)

substitutions = [ ( r'\(record length: \d+, key length: \d+\)', 'record length: N, key length: M' ) ]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        for test_sql in qry_list:
            ps, rs =  None, None
            try:
                cur = con.cursor()
                ps = cur.prepare(test_sql)
                print(test_sql)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )

                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                rs = cur.execute(ps)
                cur_cols = cur.description
                for r in rs:
                    for i in range(0,len(cur_cols)):
                        print( cur_cols[i][0], ':', r[i] )

            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

    expected_out_4x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "TEST" as "T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ID2_ASC" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[0]}

        {qry_list[1]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "TEST" as "T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ID2_ASC" Range Scan (lower bound: 1/2, upper bound: 2/2)
        {data_list[1]}

        {qry_list[2]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_ASC" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[2]}

        {qry_list[3]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_DES" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[3]}

        {qry_list[4]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_ASC" Range Scan (upper bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (upper bound: 1/1)
        {data_list[4]}

        {qry_list[5]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_DES" Range Scan (lower bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (upper bound: 1/1)
        {data_list[5]}

        {qry_list[6]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_ASC" Range Scan (lower bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (lower bound: 1/1)
        {data_list[6]}

        {qry_list[7]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_DES" Range Scan (upper bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (lower bound: 1/1)
        {data_list[7]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "TEST" as "T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ID2_ASC" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[0]}

        {qry_list[1]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "TEST" as "T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ID2_ASC" Range Scan (lower bound: 1/2, upper bound: 2/2)
        {data_list[1]}

        {qry_list[2]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_ASC" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[2]}

        {qry_list[3]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_DES" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[3]}

        {qry_list[4]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_ASC" Range Scan (upper bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (upper bound: 1/1)
        {data_list[4]}

        {qry_list[5]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_DES" Range Scan (lower bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (upper bound: 1/1)
        {data_list[5]}

        {qry_list[6]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_ASC" Range Scan (lower bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (lower bound: 1/1)
        {data_list[6]}

        {qry_list[7]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Access By ID
        ............-> Index "TEST_ID2_ID1_DES" Range Scan (upper bound: 1/2)
        ................-> Bitmap
        ....................-> Index "TEST_ID1_ASC" Range Scan (lower bound: 1/1)
        {data_list[7]}

    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "PUBLIC"."TEST" as "T" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_ID1_ID2_ASC" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[0]}

        {qry_list[1]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "PUBLIC"."TEST" as "T" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_ID1_ID2_ASC" Range Scan (lower bound: 1/2, upper bound: 2/2)
        {data_list[1]}

        {qry_list[2]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Access By ID
        ............-> Index "PUBLIC"."TEST_ID2_ID1_ASC" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[2]}

        {qry_list[3]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Access By ID
        ............-> Index "PUBLIC"."TEST_ID2_ID1_DES" Range Scan (lower bound: 2/2, upper bound: 1/2)
        {data_list[3]}

        {qry_list[4]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Access By ID
        ............-> Index "PUBLIC"."TEST_ID2_ID1_ASC" Range Scan (upper bound: 1/2)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_ID1_ASC" Range Scan (upper bound: 1/1)
        {data_list[4]}

        {qry_list[5]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Access By ID
        ............-> Index "PUBLIC"."TEST_ID2_ID1_DES" Range Scan (lower bound: 1/2)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_ID1_ASC" Range Scan (upper bound: 1/1)
        {data_list[5]}

        {qry_list[6]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Access By ID
        ............-> Index "PUBLIC"."TEST_ID2_ID1_ASC" Range Scan (lower bound: 1/2)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_ID1_ASC" Range Scan (lower bound: 1/1)
        {data_list[6]}

        {qry_list[7]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Access By ID
        ............-> Index "PUBLIC"."TEST_ID2_ID1_DES" Range Scan (upper bound: 1/2)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_ID1_ASC" Range Scan (lower bound: 1/1)
        {data_list[7]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
