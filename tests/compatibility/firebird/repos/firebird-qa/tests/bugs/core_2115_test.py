#coding:utf-8

"""
ID:          issue-2548
ISSUE:       2548
TITLE:       Query plan is missing for the long query
DESCRIPTION: 
    test creates table with one index and generates query like:
        select * from ... where
            exists(select ...) and
            exists(select ...) and
            exists(select ...) and
            ...
    -- where number of sub-queries is defined by variable SUBQRY_COUNT
    Then we open cursor and ask to show execution plan (in traditional form).
    Plan must have SUBQRY_COUNT lines with the same content: 'PLAN (T1 INDEX (T1_X))'
JIRA:        CORE-2115
FBTEST:      bugs.core_2115
NOTES:
    [07.09.2023] pzotov
    1. Refactored because old query did use "IN(...)" predicate with lot of literals.
       This query became non-actual since IN-algorithm was changed in 
       https://github.com/FirebirdSQL/firebird/commit/0493422c9f729e27be0112ab60f77e753fabcb5b
       ("Better processing and optimization if IN <list> predicates (#7707)")
    2. Although firebird-driver and firebird-qa assumes to be used under FB 3.x+, generated script
       was also checked on FB 2.0.7.13318 and FB 2.1.7.18553. Both these old versions can NOT produce
       any output after SUBQRY_COUNT >= 97.
    3. Upper limit for SUBQRY_COUNT currently still the same: 256. After exceeding of this, we get:
       "SQLSTATE = 54001 ... -Too many Contexts of Relation/Procedure/Views. Maximum allowed is 256"
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

##################
SUBQRY_COUNT = 256
##################

init_sql = """
    create sequence g;
    recreate table t1(
       x smallint
    );
    commit;
    insert into t1 select mod(gen_id(g,1), 256) from rdb$types,(select 1 i from rdb$types rows 10);
    commit;
    create index t1_x on t1(x);
    commit;
"""
db = db_factory(init = init_sql)

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    test_sql = """
        select 1 as c
        from t1
        where x = 0 and
    """

    test_sql += '\n'.join( ( f'exists(select 1 from t1 where x = {i}) and' for i in range(SUBQRY_COUNT-1) ) )
    test_sql += '\n1=1'

    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)
            print(ps.plan)
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

    expected_stdout = '\n'.join( ('PLAN (T1 INDEX (T1_X))' for i in range(SUBQRY_COUNT)) )
    assert act.clean_stdout == act.clean_expected_stdout
