#coding:utf-8

"""
ID:          issue-4954
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/4954
TITLE:       subselect losing the index when where clause includes coalesce() [CORE4640]
DESCRIPTION:
NOTES:
    [21.08.2024] pzotov
        Confirmed bug on 2.1.7.18553. No such problem on 2.5.927156.
        Checked on 6.0.0.438, 5.0.2.1479, 4.0.6.3142, 3.0.12.33784.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = """
    recreate table t1 (
        id int not null,
        vc1 varchar(1) not null,
        i1 int not null,
        i2 int not null,
        constraint t1_pk primary key (id) using descending index t1_pk,
        constraint t1_uk1 unique (i1, i2, vc1)
    );

    recreate table t2 (
        id int not null,
        vc1 varchar(1) not null,
        i1 int not null,
        i2 int not null,
        constraint t2_pk primary key (id) using descending index t2_pk,
        constraint t2_uk1 unique (i1, i2, vc1)
    );

    recreate table t3 (
        id int not null,
        i1_1 int,
        i1_2 int
    );

    create view v1 (ID, VC1, I1, I2) as
    select t1.id, t1.vc1, t1.i1, t1.i2
      from t1
    union all
    select t2.id, t2.vc1, t2.i1, t2.i2
      from t2;
    commit;

    insert into t1 (id, vc1, i1, i2) values (9, 'a', 1009, 1000);
    insert into t1 (id, vc1, i1, i2) values (8, 'a', 1008, 1000);
    insert into t1 (id, vc1, i1, i2) values (7, 'a', 1007, 1000);
    insert into t1 (id, vc1, i1, i2) values (6, 'a', 1006, 1000);
    insert into t1 (id, vc1, i1, i2) values (5, 'a', 1005, 1000);
    insert into t1 (id, vc1, i1, i2) values (4, 'a', 1004, 1000);
    insert into t1 (id, vc1, i1, i2) values (3, 'a', 1003, 1000);
    insert into t1 (id, vc1, i1, i2) values (2, 'a', 1002, 1000);
    insert into t1 (id, vc1, i1, i2) values (1, 'a', 1001, 1000);


    insert into t2 (id, vc1, i1, i2) values (19, 'a', 1019, 1000);
    insert into t2 (id, vc1, i1, i2) values (18, 'a', 1018, 1000);
    insert into t2 (id, vc1, i1, i2) values (17, 'a', 1017, 1000);
    insert into t2 (id, vc1, i1, i2) values (16, 'a', 1016, 1000);
    insert into t2 (id, vc1, i1, i2) values (15, 'a', 1015, 1000);
    insert into t2 (id, vc1, i1, i2) values (14, 'a', 1014, 1000);
    insert into t2 (id, vc1, i1, i2) values (13, 'a', 1013, 1000);
    insert into t2 (id, vc1, i1, i2) values (12, 'a', 1012, 1000);
    insert into t2 (id, vc1, i1, i2) values (11, 'a', 1011, 1000);
    insert into t2 (id, vc1, i1, i2) values (10, 'a', 1010, 1000);


    insert into t3 (id, i1_1, i1_2) values (100000, null, 1010);
    insert into t3 (id, i1_1, i1_2) values (100001, 1012, null);
    commit;
    set statistics index t1_pk;
    set statistics index t2_pk;
    set statistics index t1_uk1;
    set statistics index t2_uk1;
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    test_sql = """
        select t3.id,
               (select first 1 v1.id
                  from v1
                  where
                      v1.vc1 = 'A'
                      and v1.i2 = 1000
                      and v1.i1 = coalesce(t3.i1_1, t3.i1_2)
               )
        from t3
    """

    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()
    
    expected_stdout_4x = """
        Select Expression
        ....-> Singularity Check
        ........-> First N Records
        ............-> Filter
        ................-> Union
        ....................-> Filter
        ........................-> Table "T1" as "V1 T1" Access By ID
        ............................-> Bitmap
        ................................-> Index "T1_UK1" Unique Scan
        ....................-> Filter
        ........................-> Table "T2" as "V1 T2" Access By ID
        ............................-> Bitmap
        ................................-> Index "T2_UK1" Unique Scan
        Select Expression
        ....-> Table "T3" Full Scan
    """

    expected_stdout_5x = """
        Sub-query
        ....-> Singularity Check
        ........-> First N Records
        ............-> Filter
        ................-> Filter
        ....................-> Union
        ........................-> Filter
        ............................-> Table "T1" as "V1 T1" Access By ID
        ................................-> Bitmap
        ....................................-> Index "T1_UK1" Unique Scan
        ........................-> Filter
        ............................-> Table "T2" as "V1 T2" Access By ID
        ................................-> Bitmap
        ....................................-> Index "T2_UK1" Unique Scan
        Select Expression
        ....-> Table "T3" Full Scan
    """
    
    expected_stdout_6x = """
        Sub-query
        ....-> Singularity Check
        ........-> First N Records
        ............-> Filter
        ................-> Filter
        ....................-> Union
        ........................-> Filter
        ............................-> Table "PUBLIC"."T1" as "PUBLIC"."V1" "PUBLIC"."T1" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PUBLIC"."T1_UK1" Unique Scan
        ........................-> Filter
        ............................-> Table "PUBLIC"."T2" as "PUBLIC"."V1" "PUBLIC"."T2" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PUBLIC"."T2_UK1" Unique Scan
        Select Expression
        ....-> Table "PUBLIC"."T3" Full Scan
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
