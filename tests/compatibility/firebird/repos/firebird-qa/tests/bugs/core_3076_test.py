#coding:utf-8

"""
ID:          issue-3455
ISSUE:       3455
TITLE:       Better performance for (table.field = :param OR :param = -1) in where clause
DESCRIPTION:
JIRA:        CORE-3076
FBTEST:      bugs.core_3076
NOTES:
    [24.07.2025] pzotov
    Re-implemented: no need to use mon$tables, all data can be obtained using con.info.get_table_access_stats().
    Explained plans have beed added in expected out.
    
    Partial indices (5.x+) can not be used in queries like "select ... from test where a = ? or ? is null"
    because actual value for parameter is unknown at prepare phase. One need to replace "?" with literal for that
    but that is not related to this test. Letter from dimitr 25.07.2025 09:13.

    Checked on 6.0.0.1061; 5.0.3.1686; 4.0.6.3223; 3.0.13.33818.
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

        create sequence g;
        recreate table test(x int, y int, u int, v int, a int, b int);
        commit;

        set term ^;
        execute block as
            declare n int = 50000;
        begin
            while (n > 0) do
            begin
                insert into test(x,y,u,v,a,b) values( 
                    mod( :n, 17 )
                   ,mod( :n, 19 )
                   ,mod( :n, 23 )
                   ,mod( :n, 29 )
                   ,mod( :n, 31 )
                   ,mod( :n, 37 )
                );
                n = n - 1;
            end
        end
        ^
        set term ;^
        commit;

        -- common indices, single-column:
        create index test_x_asc on test(x);
        create descending index test_y_dec on test(y);

        -- compound indices:
        create index test_compound_asc on test(u,x);
        create descending index test_compound_dec on test(v,y);

        create index test_computed_x_y_asc on test computed by (x+y);
        create index test_computed_x_y_dec on test computed by (x-y);
        commit;
    """

    act.isql(switches = ['-q'], input = init_script, combine_output = True)
    assert act.clean_stdout == '', 'Init script FAILED: {act.clean_stdout=}'
    act.reset()

    qry_map = {
        # test common index, asc:
        1 : ( "select /* trace_me */ count(*) from test where x = ? or ? is null",                 (1,0) ),
        2 : ( "select /* trace_me */ count(*) from test where x in (?, ?) or ? is null",           (1,2,0) ),
        3 : ( "select /* trace_me */ count(*) from test where x between ? and ? or ? is null",     (1,2,0) ),

        # test common index, desc:
        4 : ( "select /* trace_me */ count(*) from test where y = ? or ? is null",                 (1,0) ),
        5 : ( "select /* trace_me */ count(*) from test where y in (?, ?) or ? is null",           (1,2,0) ),
        6 : ( "select /* trace_me */ count(*) from test where y between ? and ? or ? is null",     (1,2,0) ),

        # test compound index, asc:
       11 : ( "select /* trace_me */ count(*) from test where u = ? or ? is null",                 (1,0) ),
       12 : ( "select /* trace_me */ count(*) from test where u in (?, ?) or ? is null",           (1,2,0) ),
       13 : ( "select /* trace_me */ count(*) from test where u between ? and ? or ? is null",     (1,2,0) ),

        # test compound index, desc:
       14 : ( "select /* trace_me */ count(*) from test where u = ? or ? is null",                 (1,0) ),
       15 : ( "select /* trace_me */ count(*) from test where u in (?, ?) or ? is null",           (1,2,0) ),
       16 : ( "select /* trace_me */ count(*) from test where u between ? and ? or ? is null",     (1,2,0) ),

        # test computed-by index, asc:
       21 : ( "select /* trace_me */ count(*) from test where x + y = ? or ? is null",             (2,0) ),
       22 : ( "select /* trace_me */ count(*) from test where x + y in (?, ?) or ? is null",       (1,2,0) ),
       23 : ( "select /* trace_me */ count(*) from test where x + y between ? and ? or ? is null", (1,2,0) ),

        # test computed-by index, desc:
       24 : ( "select /* trace_me */ count(*) from test where x - y = ? or ? is null",             (-1,0) ),
       25 : ( "select /* trace_me */ count(*) from test where x - y in (?, ?) or ? is null",       (-1,0,0) ),
       26 : ( "select /* trace_me */ count(*) from test where x - y between ? and ? or ? is null", (-1,0,0) ),
    }

    for qry_idx,v in qry_map.items():
        qry_text, qry_args = v[:2]
        qry_map[qry_idx] = (qry_text, qry_args, f'{qry_idx=} '+qry_text )

    with act.db.connect() as con:
        cur = con.cursor()

        cur.execute(f"select rdb$relation_id from rdb$relations where rdb$relation_name = upper('test')")
        test_rel_id = None
        for r in cur:
            test_rel_id = r[0]
        assert test_rel_id, f"Could not find ID for relation 'TEST'. Check its name!"

        result_map = {}
        for qry_idx, qry_data in qry_map.items():
            qry_text, qry_args, qry_comment = qry_data[:3]
            ps, rs =  None, None
            try:
                cur = con.cursor()
                ps = cur.prepare(qry_text)
                print(qry_comment)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )

                tabstat1 = [ p for p in con.info.get_table_access_stats() if p.table_id == test_rel_id ]
                print(f'{qry_args=}')
                rs = cur.execute(ps, qry_args)
                for r in rs:
                    pass

                tabstat2 = [ p for p in con.info.get_table_access_stats() if p.table_id == test_rel_id ]
                result_map[qry_idx] = \
                    (
                       tabstat2[0].sequential if tabstat2[0].sequential else 0
                      ,tabstat2[0].indexed if tabstat2[0].indexed else 0
                    )
                if tabstat1:
                    seq, idx = result_map[qry_idx]
                    seq -= (tabstat1[0].sequential if tabstat1[0].sequential else 0)
                    idx -= (tabstat1[0].indexed if tabstat1[0].indexed else 0)
                    result_map[qry_idx] = (seq, idx)

                print(f'Table statistics: NATURAL reads: {result_map[qry_idx][0]}; INDEXED reads: {result_map[qry_idx][1]}')
                print('##############################################################')

            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()


    expected_stdout_4x = f"""
        {qry_map[ 1][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_X_ASC" Range Scan (full match)
        qry_args={qry_map[ 1][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2942
        ##############################################################
        {qry_map[ 2][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap Or
        ........................-> Bitmap
        ............................-> Index "TEST_X_ASC" Range Scan (full match)
        ........................-> Bitmap
        ............................-> Index "TEST_X_ASC" Range Scan (full match)
        qry_args={qry_map[ 2][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5884
        ##############################################################
        {qry_map[ 3][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_X_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[ 3][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5884
        ##############################################################
        {qry_map[ 4][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_Y_DEC" Range Scan (full match)
        qry_args={qry_map[ 4][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2632
        ##############################################################
        {qry_map[ 5][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap Or
        ........................-> Bitmap
        ............................-> Index "TEST_Y_DEC" Range Scan (full match)
        ........................-> Bitmap
        ............................-> Index "TEST_Y_DEC" Range Scan (full match)
        qry_args={qry_map[ 5][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5264
        ##############################################################
        {qry_map[ 6][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_Y_DEC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[ 6][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5264
        ##############################################################
        {qry_map[11][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[11][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2174
        ##############################################################
        {qry_map[12][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap Or
        ........................-> Bitmap
        ............................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        ........................-> Bitmap
        ............................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[12][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[13][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (lower bound: 1/2, upper bound: 1/2)
        qry_args={qry_map[13][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[14][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[14][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2174
        ##############################################################
        {qry_map[15][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap Or
        ........................-> Bitmap
        ............................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        ........................-> Bitmap
        ............................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[15][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[16][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (lower bound: 1/2, upper bound: 1/2)
        qry_args={qry_map[16][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[21][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_ASC" Range Scan (full match)
        qry_args={qry_map[21][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 464
        ##############################################################
        {qry_map[22][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap Or
        ........................-> Bitmap
        ............................-> Index "TEST_COMPUTED_X_Y_ASC" Range Scan (full match)
        ........................-> Bitmap
        ............................-> Index "TEST_COMPUTED_X_Y_ASC" Range Scan (full match)
        qry_args={qry_map[22][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 774
        ##############################################################
        {qry_map[23][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[23][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 774
        ##############################################################
        {qry_map[24][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_DEC" Range Scan (full match)
        qry_args={qry_map[24][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2635
        ##############################################################
        {qry_map[25][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap Or
        ........................-> Bitmap
        ............................-> Index "TEST_COMPUTED_X_Y_DEC" Range Scan (full match)
        ........................-> Bitmap
        ............................-> Index "TEST_COMPUTED_X_Y_DEC" Range Scan (full match)
        qry_args={qry_map[25][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5269
        ##############################################################
        {qry_map[26][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_DEC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[26][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5269
        ##############################################################
    """

    expected_stdout_5x = f"""
        {qry_map[ 1][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_X_ASC" Range Scan (full match)
        qry_args={qry_map[ 1][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2942
        ##############################################################
        {qry_map[ 2][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_X_ASC" List Scan (full match)
        qry_args={qry_map[ 2][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5884
        ##############################################################
        {qry_map[ 3][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_X_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[ 3][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5884
        ##############################################################
        {qry_map[ 4][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_Y_DEC" Range Scan (full match)
        qry_args={qry_map[ 4][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2632
        ##############################################################
        {qry_map[ 5][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_Y_DEC" List Scan (full match)
        qry_args={qry_map[ 5][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5264
        ##############################################################
        {qry_map[ 6][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_Y_DEC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[ 6][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5264
        ##############################################################
        {qry_map[11][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[11][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2174
        ##############################################################
        {qry_map[12][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" List Scan (partial match: 1/2)
        qry_args={qry_map[12][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[13][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (lower bound: 1/2, upper bound: 1/2)
        qry_args={qry_map[13][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[14][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[14][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2174
        ##############################################################
        {qry_map[15][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" List Scan (partial match: 1/2)
        qry_args={qry_map[15][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[16][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPOUND_ASC" Range Scan (lower bound: 1/2, upper bound: 1/2)
        qry_args={qry_map[16][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[21][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_ASC" Range Scan (full match)
        qry_args={qry_map[21][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 464
        ##############################################################
        {qry_map[22][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_ASC" List Scan (full match)
        qry_args={qry_map[22][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 774
        ##############################################################
        {qry_map[23][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[23][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 774
        ##############################################################
        {qry_map[24][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_DEC" Range Scan (full match)
        qry_args={qry_map[24][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2635
        ##############################################################
        {qry_map[25][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_DEC" List Scan (full match)
        qry_args={qry_map[25][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5269
        ##############################################################
        {qry_map[26][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "TEST" Full Scan
        ................-> Table "TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "TEST_COMPUTED_X_Y_DEC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[26][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5269
        ##############################################################
    """

    expected_stdout_6x = f"""
        {qry_map[ 1][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)
        qry_args={qry_map[ 1][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2942
        ##############################################################
        {qry_map[ 2][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_X_ASC" List Scan (full match)
        qry_args={qry_map[ 2][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5884
        ##############################################################
        {qry_map[ 3][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[ 3][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5884
        ##############################################################
        {qry_map[ 4][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_Y_DEC" Range Scan (full match)
        qry_args={qry_map[ 4][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2632
        ##############################################################
        {qry_map[ 5][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_Y_DEC" List Scan (full match)
        qry_args={qry_map[ 5][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5264
        ##############################################################
        {qry_map[ 6][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_Y_DEC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[ 6][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5264
        ##############################################################
        {qry_map[11][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[11][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2174
        ##############################################################
        {qry_map[12][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPOUND_ASC" List Scan (partial match: 1/2)
        qry_args={qry_map[12][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[13][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPOUND_ASC" Range Scan (lower bound: 1/2, upper bound: 1/2)
        qry_args={qry_map[13][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[14][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPOUND_ASC" Range Scan (partial match: 1/2)
        qry_args={qry_map[14][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2174
        ##############################################################
        {qry_map[15][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPOUND_ASC" List Scan (partial match: 1/2)
        qry_args={qry_map[15][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[16][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPOUND_ASC" Range Scan (lower bound: 1/2, upper bound: 1/2)
        qry_args={qry_map[16][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 4348
        ##############################################################
        {qry_map[21][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPUTED_X_Y_ASC" Range Scan (full match)
        qry_args={qry_map[21][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 464
        ##############################################################
        {qry_map[22][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPUTED_X_Y_ASC" List Scan (full match)
        qry_args={qry_map[22][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 774
        ##############################################################
        {qry_map[23][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPUTED_X_Y_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[23][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 774
        ##############################################################
        {qry_map[24][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPUTED_X_Y_DEC" Range Scan (full match)
        qry_args={qry_map[24][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 2635
        ##############################################################
        {qry_map[25][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPUTED_X_Y_DEC" List Scan (full match)
        qry_args={qry_map[25][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5269
        ##############################################################
        {qry_map[26][2]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Condition
        ................-> Table "PUBLIC"."TEST" Full Scan
        ................-> Table "PUBLIC"."TEST" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TEST_COMPUTED_X_Y_DEC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        qry_args={qry_map[26][1]}
        Table statistics: NATURAL reads: 0; INDEXED reads: 5269
        ##############################################################
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

