#coding:utf-8

"""
ID:          issue-1966
ISSUE:       1966
TITLE:       Subquery-based predicates are not evaluated early in the join order
DESCRIPTION:
JIRA:        CORE-1549
FBTEST:      bugs.core_1549
NOTES:
    [25.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Re-implemented in order to preserve leading spaces in the explained plans output.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    recreate table t(id int);
    commit;
    insert into t select row_number()over() from rdb$types a, (select 1 i from rdb$types rows 4) b rows 1000;
    commit;
    create index t_id on t(id);
    commit;
"""
db = db_factory(init = init_script)

qry_map = {
    1000 :
    (
        """
            select a.id a_id, b.id b_id
            from t a join t b on b.id >= a.id
            where
                not exists (select * from t x where x.id = a.id - 1)
                and
                not exists (select * from t z where z.id = b.id + 1);
        """
        ,
        "EXISTS() with reference to 1st stream"
    )
    ,
    2000 :
    (
        """
            select a.id a_id, b.id b_id
            from (
                select t1.id
                from t t1
                where
                    not exists (select * from t x where x.id = t1.id - 1)
            ) a
            join
            (
                select t2.id
                from t t2
                where
                    not exists (select * from t x where x.id = t2.id + 1)
            ) b
            on b.id >= a.id
        """
        ,
        'Two separate derived tables and EXISTS() inside. Its plan must be considered as "etalone", i.e. how it should be in first the query'
    )
}

act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
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

    expected_out_3x = f"""

        {qry_map.get(1000)[1]}

        Select Expression
        ....-> Filter
        ........-> Table "T" as "X" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "T" as "Z" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "T" as "A" Full Scan
        ........-> Filter
        ............-> Table "T" as "B" Access By ID
        ................-> Bitmap
        ....................-> Index "T_ID" Range Scan (lower bound: 1/1)
        
        {qry_map.get(2000)[1]}
        Select Expression
        ....-> Filter
        ........-> Table "T" as "B X" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "T" as "A X" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "T" as "A T1" Full Scan
        ........-> Filter
        ............-> Table "T" as "B T2" Access By ID
        ................-> Bitmap
        ....................-> Index "T_ID" Range Scan (lower bound: 1/1)
    """

    expected_out_5x = f"""

        {qry_map.get(1000)[1]}
        Sub-query
        ....-> Filter
        ........-> Table "T" as "X" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Sub-query
        ....-> Filter
        ........-> Table "T" as "Z" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "T" as "A" Full Scan
        ........-> Filter
        ............-> Table "T" as "B" Access By ID
        ................-> Bitmap
        ....................-> Index "T_ID" Range Scan (lower bound: 1/1)

        {qry_map.get(2000)[1]}
        Sub-query
        ....-> Filter
        ........-> Table "T" as "B X" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Sub-query
        ....-> Filter
        ........-> Table "T" as "A X" Access By ID
        ............-> Bitmap
        ................-> Index "T_ID" Range Scan (full match)
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "T" as "A T1" Full Scan
        ........-> Filter
        ............-> Table "T" as "B T2" Access By ID
        ................-> Bitmap
        ....................-> Index "T_ID" Range Scan (lower bound: 1/1)
    """

    expected_out_6x = f"""

        {qry_map.get(1000)[1]}
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."T" as "X" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."T_ID" Range Scan (full match)
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."T" as "Z" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."T_ID" Range Scan (full match)
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "PUBLIC"."T" as "A" Full Scan
        ........-> Filter
        ............-> Table "PUBLIC"."T" as "B" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."T_ID" Range Scan (lower bound: 1/1)

        {qry_map.get(2000)[1]}
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."T" as "B" "X" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."T_ID" Range Scan (full match)
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."T" as "A" "X" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."T_ID" Range Scan (full match)
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "PUBLIC"."T" as "A" "T1" Full Scan
        ........-> Filter
        ............-> Table "PUBLIC"."T" as "B" "T2" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."T_ID" Range Scan (lower bound: 1/1)
    """

    act.expected_stdout = expected_out_3x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
