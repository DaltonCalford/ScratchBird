#coding:utf-8

"""
ID:          issue-8109
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8109
TITLE:       Plan/Performance regression when using special construct for IN in FB5.x compared to FB3.x
DESCRIPTION:
NOTES:
    [03.02.2025] pzotov
        Confirmed problem (regression) on 6.0.0.595-2c5b146, 5.0.2.1601-f094936
        Checked on 6.0.0.601-5dee439, 5.0.2.1606-fd31e52 (intermediate snapshots).
    [06.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_sql = """
    create table test(x int, y int, u int, v int, p int, q int);
    insert into test(x,y, u,v, p,q) select r,r, r,r, r,r from ( select rand()*10000 r from rdb$types, rdb$types );
    commit;
    create index test_x_asc on test(x);
    create index test_y_asc on test(y);
    create index test_c_asc on test computed by (x+y);
    create index test_p_asc on test(p) where p < 5001; -- partial index
    create index test_q_asc on test(q) where q > 4999; -- partial index

    create descending index test_u_dec on test(u);
    create descending index test_v_dec on test(v);
    create descending index test_c_dec on test computed by (u-v);
    create descending index test_p_dec on test(p) where p > 4999; -- partial index
    create descending index test_q_dec on test(q) where q < 5001; -- partial index
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):
    queries_map = { i : x for i,x in enumerate
                      (
                          [
                               'select * from test where 5000 in (x, y)'
                              ,'select * from test where 5000 in (u, v)'
                              ,'select * from test where 5000 in (x, u)'
                              ,'select * from test where 5000 in (v, y)'
                              ,'select * from test where 5000 in (x+y, u-v)'
                              ,'select * from test where 5000 in (p, q) and p < 5001 and q > 4999'
                              ,'select * from test where 5000 in (p, q) and p > 4999 and q < 5001'
                          ]
                      )
                  }
    with act.db.connect() as con:
        cur = con.cursor()
        for qry_idx, qry_txt in queries_map.items():
            ps = None
            try:
                ps = cur.prepare(qry_txt)

                # Print explained plan with padding eash line by dots in order to see indentations:
                print(qry_txt)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                print('\n')
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()


    expected_stdout_5x = f"""
        {queries_map[ 0 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_X_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_Y_ASC" Range Scan (full match)

        {queries_map[ 1 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_U_DEC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_V_DEC" Range Scan (full match)

        {queries_map[ 2 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_X_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_U_DEC" Range Scan (full match)

        {queries_map[ 3 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_V_DEC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_Y_ASC" Range Scan (full match)

        {queries_map[ 4 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_C_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_C_DEC" Range Scan (full match)

        {queries_map[ 5 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_P_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_Q_ASC" Range Scan (full match)

        {queries_map[ 6 ]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TEST_P_DEC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TEST_Q_DEC" Range Scan (full match)
    """

    expected_stdout_6x = f"""
        {queries_map[ 0 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_Y_ASC" Range Scan (full match)

        {queries_map[ 1 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_U_DEC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_V_DEC" Range Scan (full match)

        {queries_map[ 2 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_X_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_U_DEC" Range Scan (full match)

        {queries_map[ 3 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_V_DEC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_Y_ASC" Range Scan (full match)

        {queries_map[ 4 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_C_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_C_DEC" Range Scan (full match)

        {queries_map[ 5 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_P_ASC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_Q_ASC" Range Scan (full match)

        {queries_map[ 6 ]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_P_DEC" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_Q_DEC" Range Scan (full match)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
