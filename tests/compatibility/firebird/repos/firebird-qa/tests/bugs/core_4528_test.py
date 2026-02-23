#coding:utf-8

"""
ID:          issue-4846
ISSUE:       4846
TITLE:       Allow hash/merge joins for non-field (dbkey or derived expression) equalities
DESCRIPTION:
  NB: currently join oth three and more sources made by USING or NATURAL clauses will use NL.
  See CORE-4809
JIRA:        CORE-4528
FBTEST:      bugs.core_4528
NOTES:
    [07.04.2022] pzotov
    FB 5.0.0.455 and later: data sources with equal cardinality now present in the HASH plan in order they are specified in the query.
    Reversed order was used before this build. Because of this, two cases of expected stdout must be taken in account, see variables
    'fb3x_checked_stdout' and 'fb5x_checked_stdout'.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tn(x int primary key using index tn_x);
    commit;
    insert into tn select row_number()over() from rdb$types;
    commit;

    -- this will NOT pruduce HJ, at least for WI-T3.0.0.31840
    -- select * from (select rdb$db_key a from tn) r join (select rdb$db_key b from tn) s on r.a||'' = s.b||'';

    set planonly;
    --set echo on;

    -- ### 1. Columns from derived tables ###

    select count(*)
    from (select x a from tn) r
    join (select x a from tn) s on r.a + 0 = s.a + 0;

    select count(*)
    from (select x a from tn) r
    join (select x a from tn) s on r.a + 0 = s.a + 0
    join (select x a from tn) t on s.a + 0 = t.a + 0;

    -- ### 2. RDB$DB_KEY ###

    ----------- test `traditional` join  form -----------------
    select count(*)
    from (select rdb$db_key||'' a from tn) r
    join (select rdb$db_key||'' a from tn) s on r.a = s.a;

    select count(*)
    from (select rdb$db_key||'' a from tn) r
    join (select rdb$db_key||'' a from tn) s on r.a = s.a
    join (select rdb$db_key||'' a from tn) t on s.a = t.a;

    select count(*)
    from (select rdb$db_key||'' a from tn) r
    join (select rdb$db_key||'' a from tn) s on r.a = s.a
    join (select rdb$db_key||'' a from tn) t on s.a = t.a
    join (select rdb$db_key||'' a from tn) u on t.a = u.a;

    ----------- test join on named columns form -----------------
    select count(*)
    from (select rdb$db_key||'' a from tn) r
    join (select rdb$db_key||'' a from tn) s using(a);

    -- Currently these will produce NESTED LOOPS, SEE CORE-4809.
    -- Uncomment these statements and correct expected_output when
    -- (and if) core-4809 will be fixed:
    --select count(*)
    --from (select rdb$db_key||'' a from tn) r
    --join (select rdb$db_key||'' a from tn) s using(a)
    --join (select rdb$db_key||'' a from tn) t using(a);
    --
    --select count(*)
    --from (select rdb$db_key||'' a from tn) r
    --join (select rdb$db_key||'' a from tn) s using(a)
    --join (select rdb$db_key||'' a from tn) t using(a)
    --join (select rdb$db_key||'' a from tn) u using(a);

    ----------- test natural join form -----------------

    select count(*)
    from (select rdb$db_key||'' a from tn) r
    natural join (select rdb$db_key||'' a from tn) s;

    -- Currently these will produce NESTED LOOPS, SEE CORE-4809.
    -- Uncomment these statements and correct expected_output when
    -- (and if) core-4809 will be fixed:
    --select count(*)
    --from (select rdb$db_key||'' a from tn) r
    --natural join (select rdb$db_key||'' a from tn) s
    --natural join (select rdb$db_key||'' a from tn) t;
    --
    --select count(*)
    --from (select rdb$db_key||'' a from tn) r
    --natural join (select rdb$db_key||'' a from tn) s
    --natural join (select rdb$db_key||'' a from tn) t
    --natural join (select rdb$db_key||'' a from tn) u;
"""

act = isql_act('db', test_script)

fb3x_checked_stdout = """
    PLAN HASH (S TN NATURAL, R TN NATURAL)
    PLAN HASH (HASH (T TN NATURAL, S TN NATURAL), R TN NATURAL)
    PLAN HASH (S TN NATURAL, R TN NATURAL)
    PLAN HASH (HASH (T TN NATURAL, S TN NATURAL), R TN NATURAL)
    PLAN HASH (HASH (HASH (U TN NATURAL, T TN NATURAL), S TN NATURAL), R TN NATURAL)
    PLAN HASH (S TN NATURAL, R TN NATURAL)
    PLAN HASH (S TN NATURAL, R TN NATURAL)
"""


fb5x_checked_stdout = """
    PLAN HASH (R TN NATURAL, S TN NATURAL)
    PLAN HASH (R TN NATURAL, S TN NATURAL, T TN NATURAL)
    PLAN HASH (R TN NATURAL, S TN NATURAL)
    PLAN HASH (R TN NATURAL, S TN NATURAL, T TN NATURAL)
    PLAN HASH (R TN NATURAL, S TN NATURAL, T TN NATURAL, U TN NATURAL)
    PLAN HASH (R TN NATURAL, S TN NATURAL)
    PLAN HASH (R TN NATURAL, S TN NATURAL)
"""

fb6x_checked_stdout = """
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL)
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL, "T" "PUBLIC"."TN" NATURAL)
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL)
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL, "T" "PUBLIC"."TN" NATURAL)
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL, "T" "PUBLIC"."TN" NATURAL, "U" "PUBLIC"."TN" NATURAL)
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL)
    PLAN HASH ("R" "PUBLIC"."TN" NATURAL, "S" "PUBLIC"."TN" NATURAL)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

