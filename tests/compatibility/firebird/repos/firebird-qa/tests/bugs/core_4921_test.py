#coding:utf-8

"""
ID:          issue-1956
ISSUE:       1956
TITLE:       Predicate IS [NOT] DISTINCT FROM is not pushed into unions/aggregates thus causing sub-optimal plans
DESCRIPTION:
    Implementation for 3.0 does NOT use 'set explain on' (decision after discuss with Dmitry, letter 02-sep-2015 15:42).
    Test only checks that:
    1) in case when NATURAL scan occured currently index T*_SINGLE_X is used;
    2) in case when it was only PARTIAL matching index Y*_COMPOUND_X is in use.
JIRA:        CORE-4921
FBTEST:      bugs.core_4921
NOTES:
    [30.06.2025] pzotov
    Re-implemented. Explained form is used for all checked FB versions, including 3.x

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    create or alter view v_test as select 1 id from rdb$database;
    commit;

    recreate table t1(x int, y int);
    create index t1_single_x on t1(x);
    create index t1_compound_x_y on t1(x, y);

    recreate table t2(x int, y int);
    create index t2_single_x on t2(x);
    create index t2_compound_x_y on t2(x, y);

    recreate table t3(x int, y int);
    create index t3_single_x on t3(x);
    create index t3_compound_x_y on t3(x, y);
    commit;

    create or alter view v_test as
    select * from t1
    union all
    select * from t2
    union all
    select * from t3;
    commit;
"""

db = db_factory(init = init_script)

act = python_act('db', substitutions=[('record length.*', ''), ('key length.*', '')])

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    qry_map = {
        1000 :
        """
            select * from v_test where x is not distinct from 1
        """
        ,
        2000 :
        """
            select * from v_test where x = 1 and y is not distinct from 1
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
        ....-> Filter
        ........-> Union
        ............-> Filter
        ................-> Table "T1" as "V_TEST T1" Access By ID
        ....................-> Bitmap
        ........................-> Index "T1_SINGLE_X" Range Scan (full match)
        ............-> Filter
        ................-> Table "T2" as "V_TEST T2" Access By ID
        ....................-> Bitmap
        ........................-> Index "T2_SINGLE_X" Range Scan (full match)
        ............-> Filter
        ................-> Table "T3" as "V_TEST T3" Access By ID
        ....................-> Bitmap
        ........................-> Index "T3_SINGLE_X" Range Scan (full match)


        {qry_map[2000]}
        Select Expression
        ....-> Filter
        ........-> Union
        ............-> Filter
        ................-> Table "T1" as "V_TEST T1" Access By ID
        ....................-> Bitmap
        ........................-> Index "T1_COMPOUND_X_Y" Range Scan (full match)
        ............-> Filter
        ................-> Table "T2" as "V_TEST T2" Access By ID
        ....................-> Bitmap
        ........................-> Index "T2_COMPOUND_X_Y" Range Scan (full match)
        ............-> Filter
        ................-> Table "T3" as "V_TEST T3" Access By ID
        ....................-> Bitmap
        ........................-> Index "T3_COMPOUND_X_Y" Range Scan (full match)
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Filter
        ........-> Union
        ............-> Filter
        ................-> Table "PUBLIC"."T1" as "PUBLIC"."V_TEST" "PUBLIC"."T1" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."T1_SINGLE_X" Range Scan (full match)
        ............-> Filter
        ................-> Table "PUBLIC"."T2" as "PUBLIC"."V_TEST" "PUBLIC"."T2" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."T2_SINGLE_X" Range Scan (full match)
        ............-> Filter
        ................-> Table "PUBLIC"."T3" as "PUBLIC"."V_TEST" "PUBLIC"."T3" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."T3_SINGLE_X" Range Scan (full match)

        {qry_map[2000]}
        Select Expression
        ....-> Filter
        ........-> Union
        ............-> Filter
        ................-> Table "PUBLIC"."T1" as "PUBLIC"."V_TEST" "PUBLIC"."T1" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."T1_COMPOUND_X_Y" Range Scan (full match)
        ............-> Filter
        ................-> Table "PUBLIC"."T2" as "PUBLIC"."V_TEST" "PUBLIC"."T2" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."T2_COMPOUND_X_Y" Range Scan (full match)
        ............-> Filter
        ................-> Table "PUBLIC"."T3" as "PUBLIC"."V_TEST" "PUBLIC"."T3" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."T3_COMPOUND_X_Y" Range Scan (full match)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
