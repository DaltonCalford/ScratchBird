#coding:utf-8

"""
ID:          issue-4608
ISSUE:       4608
TITLE:       Choose the best matching index for navigation
DESCRIPTION:
JIRA:        CORE-4285
FBTEST:      bugs.core_4285
NOTES:
    [29.06.2025] pzotov
    Re-implemented: use f-notation and dictionary with queries which SQL will be substituted in the expected output.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    set bail on;
    recreate table test (col1 int, col2 int, col3 int);
    commit;
    insert into test(col1, col2, col3)
    with recursive
    r as (
        select 0 as i from rdb$database
        union all
        select r.i+1 from r
        where r.i < 49
    )
    select mod(r1.i,1000), mod(r1.i,100), mod(r1.i,10)
    from r as r1, r as r2
    where 1=1
    ;
    commit;

    create index test_col1 on test (col1);
    create index test_col12 on test (col1, col2);
    create index test_col21 on test (col2, col1);
    create index test_col123 on test (col1, col2, col3);
    create index test_col132 on test (col1, col3, col2);
    commit;
"""

db = db_factory(init=init_script)
act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    qry_map = {
        1000 : 'select 1 from test order by col1'
       ,1500 : 'select 1 from test where col1 = 0 order by col1'
       ,2000 : 'select 1 from test order by col1, col2'
       ,2500 : 'select 1 from test where col1 = 0 order by col1, col2'
       ,3000 : 'select 1 from test where col1 = 0 and col2 = 0 order by col1, col2'
       ,3500 : 'select 1 from test order by col1, col2, col3'
       ,4000 : 'select 1 from test where col1 = 0 order by col1, col2, col3'
       ,4500 : 'select 1 from test where col1 = 0 and col2 = 0 order by col1, col2, col3'
       ,5000 : 'select 1 from test where col1 = 0 and col3 = 0 order by col1'
       ,5500 : 'select 1 from test where col1 = 0 and col3 = 0 order by col1, col2, col3'
       ,6000 : 'select 1 from test where col1 = 0 and col3 = 0 order by col1, col3'
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
        ....-> Table "TEST" Access By ID
        ........-> Index "TEST_COL1" Full Scan

        {qry_map[1500]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL1" Range Scan (full match)

        {qry_map[2000]}
        Select Expression
        ....-> Table "TEST" Access By ID
        ........-> Index "TEST_COL12" Full Scan

        {qry_map[2500]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL12" Range Scan (partial match: 1/2)

        {qry_map[3000]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL12" Range Scan (full match)

        {qry_map[3500]}
        Select Expression
        ....-> Table "TEST" Access By ID
        ........-> Index "TEST_COL123" Full Scan

        {qry_map[4000]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL123" Range Scan (partial match: 1/3)

        {qry_map[4500]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL123" Range Scan (partial match: 2/3)

        {qry_map[5000]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL132" Range Scan (partial match: 2/3)

        {qry_map[5500]}
        Select Expression
        ....-> Sort (record length: 44, key length: 24)
        ........-> Filter
        ............-> Table "TEST" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_COL132" Range Scan (partial match: 2/3)

        {qry_map[6000]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST" Access By ID
        ............-> Index "TEST_COL132" Range Scan (partial match: 2/3)
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Table "PUBLIC"."TEST" Access By ID
        ........-> Index "PUBLIC"."TEST_COL1" Full Scan

        {qry_map[1500]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL1" Range Scan (full match)

        {qry_map[2000]}
        Select Expression
        ....-> Table "PUBLIC"."TEST" Access By ID
        ........-> Index "PUBLIC"."TEST_COL12" Full Scan

        {qry_map[2500]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL12" Range Scan (partial match: 1/2)

        {qry_map[3000]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL12" Range Scan (full match)

        {qry_map[3500]}
        Select Expression
        ....-> Table "PUBLIC"."TEST" Access By ID
        ........-> Index "PUBLIC"."TEST_COL123" Full Scan

        {qry_map[4000]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL123" Range Scan (partial match: 1/3)

        {qry_map[4500]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL123" Range Scan (partial match: 2/3)

        {qry_map[5000]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL132" Range Scan (partial match: 2/3)

        {qry_map[5500]}
        Select Expression
        ....-> Sort (record length: 44, key length: 24)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_COL132" Range Scan (partial match: 2/3)

        {qry_map[6000]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST" Access By ID
        ............-> Index "PUBLIC"."TEST_COL132" Range Scan (partial match: 2/3)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
