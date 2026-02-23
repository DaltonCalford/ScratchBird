#coding:utf-8

"""
ID:          issue-7137
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7137
TITLE:       Optimizer regression: bad plan (HASH instead of JOIN) is chosen for some inner joins
NOTES:
    [26.04.2022] pzotov
        Confirmed bug (ineffective execution plan) on 3.0.9.33560 (09.02.2022).
        Checked on 6.0.0.336, 5.0.1.1383, 4.0.5.3086, 3.0.10.33569 (24.02.2022) - all fine.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = """
    recreate table test_c(x int);
    commit;

    recreate table test_a(
        id int unique using index test_a_pk
        ,ico int
        ,name varchar(50)
    );
    create index test_a_name on test_a(name);

    recreate table test_b(
        id int primary key using index test_b_pk
       ,ico int
       ,name varchar(50)
    );


    recreate table test_c(
        id int primary key using index test_c_pk
       ,pid_a int references test_a(id) using index test_c_fk
    );

    insert into test_a(id, ico, name)
    select row_number()over(), mod( row_number()over(), 10 ), ascii_char(64 + mod( row_number()over(), 10 ))
    from rdb$types
    rows 200;

    insert into test_b(id, ico, name)
    select row_number()over()-1, mod( row_number()over(), 10 ), ascii_char(64 + mod( row_number()over(), 10 ))
    from rdb$types, rdb$types
    rows 10000;

    insert into test_c(id, pid_a)
    select row_number()over(), 1 + mod( row_number()over(), 100 )
    from rdb$types, rdb$types
    rows 10000;
    commit;

    set statistics index test_a_pk;
    set statistics index test_a_name;
    set statistics index test_b_pk;
    set statistics index test_c_pk;
    set statistics index test_c_fk;
    commit;
"""

db = db_factory(init = init_sql)

query_lst = [
    """
        select 1
        from test_a a
        join test_b b on b.ico = a.ico
        join test_c c on c.pid_a = a.id
        where b.id = 0 and a.name = b.name
    """,
]

act = python_act('db')

#---------------------------------------------------------
def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped
#---------------------------------------------------------

@pytest.mark.version('>=3.0.9')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        for q in query_lst:
            ps = None
            try:
                ps = cur.prepare(q)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan .split('\n')]) )
            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()

    expected_stdout_5x = """
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "TEST_B" as "B" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_B_PK" Unique Scan
        ........-> Filter
        ............-> Table "TEST_A" as "A" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_A_NAME" Range Scan (full match)
        ........-> Filter
        ............-> Table "TEST_C" as "C" Access By ID
        ................-> Bitmap
        ....................-> Index "TEST_C_FK" Range Scan (full match)
    """

    expected_stdout_6x = """
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST_B" as "B" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_B_PK" Unique Scan
        ........-> Filter
        ............-> Table "PUBLIC"."TEST_A" as "A" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_A_NAME" Range Scan (full match)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST_C" as "C" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST_C_FK" Range Scan (full match)
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
