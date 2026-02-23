#coding:utf-8

"""
ID:          issue-3199
ISSUE:       3199
TITLE:       Prohibit any improper mixture of explicit and implicit joins
DESCRIPTION:
JIRA:        CORE-2812
FBTEST:      bugs.core_2812
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t_left(id int);
    insert into t_left values(111);
    insert into t_left values(999);
    commit;

    recreate table t_right(id int, val int);
    insert into t_right values(111,0);
    insert into t_right values(999,123456789);
    commit;

    recreate table t_middle(id int);
    insert into t_middle values(1);
    commit;

    -- one more sample (after discussion with Dmitry by e-mail, 02-apr-2015 19:34)
    recreate table t1(id int);
    commit;
    insert into t1 values( 1 );
    commit;

    recreate table test(x int);
    insert into test values(1);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;

    select
         'case-1' as msg
        ,L.id proc_a_id
        ,m.id mid_id
        ,R.id b_id, R.val
    from t_left L

         ,  -- ::: nb ::: this is >>> COMMA <<< instead of `cross join`

         t_middle m
         left join t_right R on L.id=R.id
    ;

    select
        'case-2' as msg
        ,l.id a_id, m.id mid_id, r.id b_id, r.val
    from t_left l
        cross join t_middle m
        left join t_right r on l.id=r.id;

    -- Added 02-apr-2015:
    select 'case-3' msg, a.id
    from t1 a
        , t1 b
          join t1 c on a.id=c.id
    where a.id=b.id; -- this FAILS on 3.0

    select 'case-4' msg, a.id
    from t1 b
       , t1 a
         join t1 c on a.id=c.id
    where a.id=b.id; -- this WORKS on 3.0

    ---------------------------------------------------------

    -- Added 29-jun-2017, after reading CORE-5573:

    -- This should PASS:
    select 1 as z1
    from
        test a
        join
              test s
              inner join
              (
                    test d
                    join test e on e.x = d.x
                    join ( test f join test g on f.x = g.x ) on e.x = g.x
                    --- and f.x=s.x

              )
              on 1=1
        on g.x=d.x
    ;

    -- This should FAIL on 3.0+ (but is passes on 2.5):
    select 2 as z2
    from
        test a
        join
              test s
              inner join
              (
                    test d
                    join test e on e.x = d.x
                    join ( test f join test g on f.x = g.x ) on e.x = g.x
                    and f.x=s.x -- <<< !! <<<

              )
              on 1=1
        on g.x=d.x
    ;


"""

substitutions = [('[ \t]+', ' '), ('-At line.*', '')]

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -L.ID
    -At line 11, column 33
    MSG                             case-2
    A_ID                            111
    MID_ID                          1
    B_ID                            111
    VAL                             0
    MSG                             case-2
    A_ID                            999
    MID_ID                          1
    B_ID                            999
    VAL                             123456789
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -A.ID
    -At line 4, column 24
    MSG                             case-4
    ID                              1
    Z1                              1
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -S.X
    -At line 11, column 29
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"L"."ID"
    -At line 11, column 33
    MSG                             case-2
    A_ID                            111
    MID_ID                          1
    B_ID                            111
    VAL                             0
    MSG                             case-2
    A_ID                            999
    MID_ID                          1
    B_ID                            999
    VAL                             123456789
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"A"."ID"
    -At line 5, column 24
    MSG                             case-4
    ID                              1
    Z1                              1
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"S"."X"
    -At line 12, column 29
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
