#coding:utf-8

"""
ID:          issue-530
ISSUE:       530
TITLE:       CREATE VIEW ignores PLAN
DESCRIPTION:
    Test verifies that:
    1. "PLAN <...>" clause inside view DLL is always ignored and actual plan will be one of following:
        1.1. That is specified explicitly by client who runs a query to this view;
        1.2. If no explicitly specified plan that optimizer will be allowed to choose that.
    2. One may to specify PLAN on client side and it *WILL* be taken in account.

    It is supposed that some view contains explicitly specified PLAN NATURAL it its DDL.
    If underlying query became suitable to be run with PLAN INDEX (e.g. such index was added to the table)
    then this 'PLAN NATURAL' will be IGNORED until it is explicitly specified in the client query.
    See below example #4 for view v_test1 defined as "select * from ... plan (t natural)".
JIRA:        CORE-203
FBTEST:      bugs.core_0203
NOTES:
    [03.07.2025] pzotov
    Re-implemented: use explained form of plans for check.
    Output is organized to be more suitable for reading and search for mismatches (see 'qry_map' dict).
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    set bail on;
    recreate table test(x int, y int);
    commit;

    insert into test(x,y) select rand()*100, rand()*10000 from rdb$types,rdb$types rows 10000;
    commit;

    create index test_x_asc on test(x);
    create descending index test_x_desc on test(x);

    create index test_y_x on test(y, x);
    create index test_x_y on test(x, y);

    create index test_sum_x_y on test computed by (x+y);
    create descending index test_sub_x_y on test computed by (x-y);

    commit;

    recreate view v_test1 as select * from test t where x = 0 plan (t natural);
    recreate view v_test2 as select * from test t where x = 0;
    recreate view v_test3 as select * from test t where x = 0 plan (t index(test_x_desc));
    recreate view v_test4 as select * from v_test3;
    commit;
"""


db = db_factory(init = init_script)

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):

    qry_map = {
          1 : ( 'select * from test t where x = 0 plan (t natural)', '' )
        , 2 : ( 'select * from v_test1 v1', '' )
        , 3 : ( 'select * from v_test1 v2', '' )
        , 4 : ( 'select * from v_test1 v1 where v1.x = 0 plan (v1 natural)', '' )
        , 5 : ( 'select * from v_test2 v2 where v2.x = 0 plan (v2 natural)', '' )
        , 6 : ( 'select * from v_test1 v1 where v1.x = 0 PLAN (V1 INDEX (TEST_X_DESC))', '' )
        , 7 : ( 'select * from v_test2 v2 where v2.x = 0 PLAN (V2 INDEX (TEST_X_DESC))', '' )
        , 8 : ( 'select * from v_test1 v1 where v1.x = 50 and v1.y = 5000 PLAN (V1 INDEX (test_x_y))', '' )
        , 9 : ( 'select * from v_test1 v2 where v2.x = 50 and v2.y = 5000 PLAN (V2 INDEX (test_y_x))', '' )
        ,10 : ( 'select * from v_test1 v1 where v1.x + v1.y = 1000 PLAN (V1 INDEX (test_x_y))', '' )
        ,11 : ( 'select * from v_test2 v2 where v2.x - v2.y = 1000 PLAN (V2 INDEX (test_x_y))', '' )
        ,12 : ( 'select * from v_test1 v1 where v1.x + v1.y = 1000 PLAN (V1 INDEX (test_sum_x_y))', '' )
        ,13 : ( 'select * from v_test2 v2 where v2.x - v2.y = 1000 PLAN (V2 INDEX (test_sub_x_y))', '' )
        ,14 : ( 'select * from v_test3 v3', 'must use index TEST_X_ASC' )
        ,15 : ( 'select * from v_test3 v3 plan ( v3 index(test_x_y) )', '' )
        ,16 : ( 'select * from v_test4 v4', 'must use index TEST_X_ASC' )
        ,17 : ( 'select * from v_test4 v4 PLAN (V4 V_TEST3 T INDEX (TEST_X_Y))', '' )
    }
    for qry_idx,v in qry_map.items():
        qry_comment = f'{qry_idx=} ' + v[1]
        qry_map[qry_idx] = (v[0], qry_comment)


    with act.db.connect() as con:
        cur = con.cursor()
        for qry_idx, qry_data in qry_map.items():
            test_sql, qry_comment = qry_data[:2]
            ps, rs =  None, None
            try:
                cur = con.cursor()
                ps = cur.prepare(test_sql)

                print(qry_comment)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()


    expected_stdout_3x = f"""
        {qry_map[ 1][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Full Scan

        {qry_map[ 2][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[ 3][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[ 4][1]}
        Select Expression
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST" as "V1 T" Full Scan

        {qry_map[ 5][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Full Scan

        {qry_map[ 6][1]}
        Select Expression
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST" as "V1 T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_X_DESC" Range Scan (full match)

        {qry_map[ 7][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_DESC" Range Scan (full match)

        {qry_map[ 8][1]}
        Select Expression
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST" as "V1 T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_X_Y" Range Scan (full match)

        {qry_map[ 9][1]}
        Select Expression
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST" as "V2 T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_Y_X" Range Scan (full match)

        {qry_map[10][1]}
        Select Expression
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST" as "V1 T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[11][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[12][1]}
        Select Expression
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST" as "V1 T" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_SUM_X_Y" Range Scan (full match)

        {qry_map[13][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_SUB_X_Y" Range Scan (full match)

        {qry_map[14][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[15][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[16][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V4 V_TEST3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[17][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V4 V_TEST3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)
    """

    expected_stdout_5x = f"""
        {qry_map[ 1][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "T" Full Scan

        {qry_map[ 2][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[ 3][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[ 4][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Full Scan

        {qry_map[ 5][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Full Scan

        {qry_map[ 6][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_DESC" Range Scan (full match)

        {qry_map[ 7][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_DESC" Range Scan (full match)

        {qry_map[ 8][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (full match)

        {qry_map[ 9][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_Y_X" Range Scan (full match)

        {qry_map[10][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[11][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[12][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V1 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_SUM_X_Y" Range Scan (full match)

        {qry_map[13][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V2 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_SUB_X_Y" Range Scan (full match)

        {qry_map[14][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[15][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[16][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V4 V_TEST3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_ASC" Range Scan (full match)

        {qry_map[17][1]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" as "V4 V_TEST3 T" Access By ID
        ............-> Bitmap
        ................-> Index "TEST_X_Y" Range Scan (partial match: 1/2)

    """

    expected_stdout_6x = f"""
        {qry_map[ 1][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "T" Full Scan

        {qry_map[ 2][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V1" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)

        {qry_map[ 3][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V2" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)

        {qry_map[ 4][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V1" "T" Full Scan

        {qry_map[ 5][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V2" "T" Full Scan

        {qry_map[ 6][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V1" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_DESC" Range Scan (full match)

        {qry_map[ 7][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V2" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_DESC" Range Scan (full match)

        {qry_map[ 8][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V1" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_Y" Range Scan (full match)

        {qry_map[ 9][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V2" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_Y_X" Range Scan (full match)

        {qry_map[10][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V1" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[11][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V2" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[12][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V1" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_SUM_X_Y" Range Scan (full match)

        {qry_map[13][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V2" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_SUB_X_Y" Range Scan (full match)

        {qry_map[14][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V3" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)

        {qry_map[15][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V3" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_Y" Range Scan (partial match: 1/2)

        {qry_map[16][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V4" "PUBLIC"."V_TEST3" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)

        {qry_map[17][1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" as "V4" "PUBLIC"."V_TEST3" "T" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TEST_X_Y" Range Scan (partial match: 1/2)
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

