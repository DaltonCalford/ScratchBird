#coding:utf-8

"""
ID:          issue-852
ISSUE:       852
TITLE:       Optimization problem with COALESCE
DESCRIPTION:
    Verify fixes for many issues Adriano found for CORE-501, CORE-1343 and CORE-2041.
NOTES:
    [25.04.2020] pzotov
    Fixed lot of bugs related to wrong count of updatable columns (they were not specified in DML).
    Replaced test_type to 'ISQL' because all can be done w/o Python calls.
    Checked on 3.0.6.33289, 4.0.0.1935.

    [18.11.2020] pzotov
    Changed expected_stderr for parametrized statement "select coalesce(1 + cast(? ...), 2 + cast(? ...)) ...":
    now it must be "-No SQLDA for input values provided" (was: "-Wrong number of parameters (expected 3, got 0)").
    Output became proper since CORE-6447 was fixed.

    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    Also, for this test 'schema:' in SQLDA output is suppressed because as not relevant to check.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
JIRA:        CORE-501
FBTEST:      bugs.core_0501
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create sequence s1;

    create table t1 (
        n integer primary key,
        x integer,
        cn computed by (coalesce(n + 0, null)),
        cx computed by (coalesce(x + 0, null))
    );

    -- test update or insert
    update or insert into t1 values (next value for s1, 10);
    update or insert into t1 values (next value for s1, 20);
    update or insert into t1 values (next value for s1, 30);

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test update or insert using coalesce
    update or insert into t1
      values (coalesce((select first 1 n from t1 order by n), null), coalesce(40 + 60, 0));

    select 'point-01' as msg, t.*
    from t1 t;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test update or insert in PSQL
    set term ^ ;
    execute block returns (msg varchar(30), n integer, x integer, cn integer, cx integer) as
        declare z integer = 200;
    begin
      update or insert into t1
        values (coalesce((select first 1 skip 1 n from t1 order by n), null), :z);

      msg = 'point-02';
      for
          select n, x, cn, cx
          from t1
          into n, x, cn, cx
      do
          suspend;
    end
    ^
    set term ;^

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test view
    create view v1 as
    select
        n   -- integer primary key
       ,x   -- integer
       ,cn  --computed by (coalesce(n + 0, null))
       ,cx  -- computed by (coalesce(x + 0, null))
       ,coalesce(n + 0, null) as vcn
    from t1;

    select 'point-03' as msg, v.*
    from v1 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test update or insert into a view
    update or insert into v1(n,x) values (next value for s1, 40);

    select 'point-04' as msg, v.*
    from v1 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test update or insert into a view in PSQL
    set term ^ ;
    execute block returns (msg varchar(30), n integer, x integer, cn integer, cx integer) as
      declare z integer = 300;
    begin
        update or insert into v1(n, x) values ( (select first 1 skip 2 n from t1 order by n), :z );

        msg = 'point-05';
        for
            select n, x, cn, cx
            from v1
            into n, x, cn, cx
        do
            suspend;
    end
    ^

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test view trigger
    create trigger v1_bi before insert on v1 as
        declare z integer = 1000;
    begin
        insert into t1(n, x) values (coalesce(new.n + :z, null), new.x);
    end
    ^
    set term ;^

    insert into v1(n, x) values (8, 88);

    select 'point-06' as msg, v.* from v1 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- several tests of COALESCE:
    select 'point-07' as msg, coalesce(n * 1, null) coal_01 from v1;
    select 'point-08' as msg, coalesce(n * 1, null) coal_02 from t1 group by coalesce(n * 1, null);
    select 'point-09' as msg, v.* from (select coalesce(n * 1, null) coal_03 from v1 group by coalesce(n * 1, null) ) v;
    select 'point-10' as msg, v.* from (select coalesce(n * 1, null) coal_04 from v1 group by 1) v;
    select 'point-11' as msg, v.* from (select coalesce(n * 1, null) coal_05 from v1 group by 1 having coalesce(n * 1, null) < 100) v;
    select 'point-12' as msg, v.* from (select coalesce(n * 10, null) coal_06 from v1 order by 1) v;
    select 'point-13' as msg, v.* from (select coalesce(n * 10, null) coal_07a, coalesce(x * 10, null) coal_07b from v1 order by 2 desc, 1 desc) v;
    select 'point-14' as msg, v.* from (select coalesce(n * 10, null) coal_08a, coalesce(x * 10, null) coal_08b from v1 order by 1 desc, 2 desc) v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test 'case when ... then ... else ... end'
    select
        'point-15' as msg
        ,v.*
    from (
        select case n * 1 when 1 then n * 1 else n + 0 end case_group_by_01
        from v1
        group by case n * 1 when 1 then n * 1 else n + 0 end
    ) v;

    select
        'point-16' as msg
        ,v.*
    from (
        select case n * 1 when 1 then n * 1 else n + 0 end case_group_by_02
        from v1 group by 1
    ) v;

    select
        'point-17' as msg
        ,v.*
    from (
        select case n * 1 when 1 then n * 1 else n + 0 end case_group_by_03
        from v1
        group by 1
        having case n * 1 when 1 then n * 1 else n + 0 end < 100
    ) v;

    select
        'point-18' as msg
        ,v.*
    from (
        select case n * 1 when 1 then n * 1 else n + 0 end case_group_by_04
        from v1
        order by 1 desc
    ) v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test non-valid statements (they must raise SQLSTATE = 42000 / -Invalid expression in the HAVING clause):

    select
        'point-19' as msg
        ,v.*
    from (
        select coalesce(n * 1, null) non_valid_01
        from v1
        group by 1 having coalesce(n * 0, null) < 100
    ) v;

    select
        'point-20' as msg
        ,v.*
    from (
        select case n * 1 when 1 then n * 1 else n + 0 end  non_valid_02
        from v1
        group by case n * 1 when 1 then n * 1 else n + 1 end
    ) v;

    select
        'point-21' as msg
        ,v.*
    from (
        select case n * 1 when 1 then n * 1 else n + 0 end  non_valid_03
        from v1
        group by 1
        having case n * 1 when 1 then n * 1 else n + 1 end < 100
    ) v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    set term ^ ;
    create procedure p1 returns (n integer) as
    begin
        -- This expression is VALID, no error should be here:
        for
            select coalesce(n * 1, null)
            from t1
            group by coalesce(n * 1, null)
            into n
        do
            suspend;
    end
    ^
    set term ; ^
    commit;

    select 'point-22' as msg, p.* from p1 p;

    -- set blob all;
    -- select rdb$procedure_blr from rdb$procedures where rdb$procedure_name = 'P1';

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test coalesce in view condition

    create view v2 as
    select
        t1.n as v2_n
        ,coalesce(n + 1, null) as v2_x1
        ,coalesce(n + 2, null) as v2_x2
    from t1
    where coalesce(0 + 0, null) = coalesce(0 + 0, null);

    select 'point-23' as msg, v.* from v2 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test coalesce in view using distinct
    create view v3 as
      select distinct
          t1.n v3_n,
          coalesce(n + 1, null) + coalesce(n + 11, null) v3_x1,
          coalesce(n + 2, null) + coalesce(n + 22, null) v3_x2
        from t1;

    select 'point-24' as msg, v.* from v3 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test coalesce with subselect with coalesce in view
    create view v4 as
      select
          t1.n v4_n,
          coalesce((select coalesce(0 + 1, null) from rdb$database), null) v4_x1,
          coalesce((select coalesce(2 + 1, null) from rdb$database), null) v4_x2
        from t1;

    select 'point-25' as msg, v.* from v4 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test coalesce in view using union
    create view v5 (n, x1, x2) as
      select
          t1.n v5_n,
          coalesce(n + 1, null) + coalesce(n + 11, null) v5_x1,
          coalesce(n + 2, null) + coalesce(n + 22, null) v5_x2
        from t1

      union all

      select
          t1.n,
          coalesce(n + 1, null) + coalesce(n + 11, null),
          coalesce(n + 2, null) + coalesce(n + 22, null)
        from t1;

    select 'point-26' as msg, v.* from v5 v;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test constraints on table COLUMNS:
    alter table t1
      add constraint t1_n check (coalesce(n + 0, null) < 10),
      add constraint t1_cx check (coalesce(cx + 0, null) < 10);

    insert into t1(n,x) values (5, 5);
    insert into t1(n,x) values (50, 5); -- violates TABLE COLUMN constraint 't1_n': value of 'n' must be < 10; SQLSTATE = 23000
    insert into t1(n,x) values (5, 50); -- violates TABLE COLUMN constraint 't1_cx': value of 'cx' must be < 10; SQLSTATE = 23000

    select 'point-27' as msg, t.* from t1 t;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test DOMAIN constraints:
    create domain dc1 as integer check (coalesce(value + 0, null) < 10);
    create domain dc2 as integer check (coalesce(value + 0, null) < 10);

    alter table t1
        add dc1 dc1,
        add dc2 dc2;

    insert into t1 (n, dc1) values (6, 6);
    insert into t1 (n, dc2) values (7, 7);
    insert into t1 (n, dc1) values (8, 10); -- violates DOMAIN constrain 'dc1'
    insert into t1 (n, dc2) values (8, 10); -- violates DOMAIN constrain 'dc2'

    select 'point-28' as msg, t.* from t1 t;

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- add bad computed expression with coalesce
    alter table t1
      add bc computed by (coalesce(n / (n - 2), null));

    select 'point-29' as msg, t.bc from t1 as t order by t.n; -- must return only one record; raises on second: SQLSTATE = 22012 / arithmetic exception... / -Integer divide by zero

    --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    -- test missed parameters:

    -- This statement will raise exception related to SQLDA.
    -- Old error messages:
    --     -SQL error code = -804
    --     -SQLDA missing or incorrect version, or incorrect number/type of variables
    -- Messages since  http://sourceforge.net/p/firebird/code/63010 (2016-02-23, DS):
    --     -SQLDA error
    --     -Wrong number of parameters (expected 3, got 0)
    -- (replacement with new text was approved by dimitr, letter 24-feb-2016 22:01).

    set sqlda_display on;
    --------------------------------------------------------------
    select
        'point-30' as msg
        ,coalesce(1 + cast(? as integer)
        ,2 + cast(? as integer))
      from rdb$database
      where coalesce(3 + cast(? as bigint), null) = 0;

"""

# NB: 'schema:' presents in the SQLDA output for FB 6.x, we can suppress it for *this* test:
substitutions = [('[ \t]+', ' '), ('table: schema: owner:', 'table: owner:')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    MSG point-01
    N 1
    X 100
    CN 1
    CX 100
    MSG point-01
    N 2
    X 20
    CN 2
    CX 20
    MSG point-01
    N 3
    X 30
    CN 3
    CX 30
    MSG point-02
    N 1
    X 100
    CN 1
    CX 100
    MSG point-02
    N 2
    X 200
    CN 2
    CX 200
    MSG point-02
    N 3
    X 30
    CN 3
    CX 30
    MSG point-03
    N 1
    X 100
    CN 1
    CX 100
    VCN 1
    MSG point-03
    N 2
    X 200
    CN 2
    CX 200
    VCN 2
    MSG point-03
    N 3
    X 30
    CN 3
    CX 30
    VCN 3
    MSG point-04
    N 1
    X 100
    CN 1
    CX 100
    VCN 1
    MSG point-04
    N 2
    X 200
    CN 2
    CX 200
    VCN 2
    MSG point-04
    N 3
    X 30
    CN 3
    CX 30
    VCN 3
    MSG point-04
    N 4
    X 40
    CN 4
    CX 40
    VCN 4
    MSG point-05
    N 1
    X 100
    CN 1
    CX 100
    MSG point-05
    N 2
    X 200
    CN 2
    CX 200
    MSG point-05
    N 3
    X 300
    CN 3
    CX 300
    MSG point-05
    N 4
    X 40
    CN 4
    CX 40
    MSG point-06
    N 1
    X 100
    CN 1
    CX 100
    VCN 1
    MSG point-06
    N 2
    X 200
    CN 2
    CX 200
    VCN 2
    MSG point-06
    N 3
    X 300
    CN 3
    CX 300
    VCN 3
    MSG point-06
    N 4
    X 40
    CN 4
    CX 40
    VCN 4
    MSG point-06
    N 1008
    X 88
    CN 1008
    CX 88
    VCN 1008
    MSG point-07
    COAL_01 1
    MSG point-07
    COAL_01 2
    MSG point-07
    COAL_01 3
    MSG point-07
    COAL_01 4
    MSG point-07
    COAL_01 1008
    MSG point-08
    COAL_02 1
    MSG point-08
    COAL_02 2
    MSG point-08
    COAL_02 3
    MSG point-08
    COAL_02 4
    MSG point-08
    COAL_02 1008
    MSG point-09
    COAL_03 1
    MSG point-09
    COAL_03 2
    MSG point-09
    COAL_03 3
    MSG point-09
    COAL_03 4
    MSG point-09
    COAL_03 1008
    MSG point-10
    COAL_04 1
    MSG point-10
    COAL_04 2
    MSG point-10
    COAL_04 3
    MSG point-10
    COAL_04 4
    MSG point-10
    COAL_04 1008
    MSG point-11
    COAL_05 1
    MSG point-11
    COAL_05 2
    MSG point-11
    COAL_05 3
    MSG point-11
    COAL_05 4
    MSG point-12
    COAL_06 10
    MSG point-12
    COAL_06 20
    MSG point-12
    COAL_06 30
    MSG point-12
    COAL_06 40
    MSG point-12
    COAL_06 10080
    MSG point-13
    COAL_07A 30
    COAL_07B 3000
    MSG point-13
    COAL_07A 20
    COAL_07B 2000
    MSG point-13
    COAL_07A 10
    COAL_07B 1000
    MSG point-13
    COAL_07A 10080
    COAL_07B 880
    MSG point-13
    COAL_07A 40
    COAL_07B 400
    MSG point-14
    COAL_08A 10080
    COAL_08B 880
    MSG point-14
    COAL_08A 40
    COAL_08B 400
    MSG point-14
    COAL_08A 30
    COAL_08B 3000
    MSG point-14
    COAL_08A 20
    COAL_08B 2000
    MSG point-14
    COAL_08A 10
    COAL_08B 1000
    MSG point-15
    CASE_GROUP_BY_01 1
    MSG point-15
    CASE_GROUP_BY_01 2
    MSG point-15
    CASE_GROUP_BY_01 3
    MSG point-15
    CASE_GROUP_BY_01 4
    MSG point-15
    CASE_GROUP_BY_01 1008
    MSG point-16
    CASE_GROUP_BY_02 1
    MSG point-16
    CASE_GROUP_BY_02 2
    MSG point-16
    CASE_GROUP_BY_02 3
    MSG point-16
    CASE_GROUP_BY_02 4
    MSG point-16
    CASE_GROUP_BY_02 1008
    MSG point-17
    CASE_GROUP_BY_03 1
    MSG point-17
    CASE_GROUP_BY_03 2
    MSG point-17
    CASE_GROUP_BY_03 3
    MSG point-17
    CASE_GROUP_BY_03 4
    MSG point-18
    CASE_GROUP_BY_04 1008
    MSG point-18
    CASE_GROUP_BY_04 4
    MSG point-18
    CASE_GROUP_BY_04 3
    MSG point-18
    CASE_GROUP_BY_04 2
    MSG point-18
    CASE_GROUP_BY_04 1
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the HAVING clause (neither an aggregate function nor a part of the GROUP BY clause)
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the HAVING clause (neither an aggregate function nor a part of the GROUP BY clause)
    MSG point-22
    N 1
    MSG point-22
    N 2
    MSG point-22
    N 3
    MSG point-22
    N 4
    MSG point-22
    N 1008
    MSG point-23
    V2_N 1
    V2_X1 2
    V2_X2 3
    MSG point-23
    V2_N 2
    V2_X1 3
    V2_X2 4
    MSG point-23
    V2_N 3
    V2_X1 4
    V2_X2 5
    MSG point-23
    V2_N 4
    V2_X1 5
    V2_X2 6
    MSG point-23
    V2_N 1008
    V2_X1 1009
    V2_X2 1010
    MSG point-24
    V3_N 1
    V3_X1 14
    V3_X2 26
    MSG point-24
    V3_N 2
    V3_X1 16
    V3_X2 28
    MSG point-24
    V3_N 3
    V3_X1 18
    V3_X2 30
    MSG point-24
    V3_N 4
    V3_X1 20
    V3_X2 32
    MSG point-24
    V3_N 1008
    V3_X1 2028
    V3_X2 2040
    MSG point-25
    V4_N 1
    V4_X1 1
    V4_X2 3
    MSG point-25
    V4_N 2
    V4_X1 1
    V4_X2 3
    MSG point-25
    V4_N 3
    V4_X1 1
    V4_X2 3
    MSG point-25
    V4_N 4
    V4_X1 1
    V4_X2 3
    MSG point-25
    V4_N 1008
    V4_X1 1
    V4_X2 3
    MSG point-26
    N 1
    X1 14
    X2 26
    MSG point-26
    N 2
    X1 16
    X2 28
    MSG point-26
    N 3
    X1 18
    X2 30
    MSG point-26
    N 4
    X1 20
    X2 32
    MSG point-26
    N 1008
    X1 2028
    X2 2040
    MSG point-26
    N 1
    X1 14
    X2 26
    MSG point-26
    N 2
    X1 16
    X2 28
    MSG point-26
    N 3
    X1 18
    X2 30
    MSG point-26
    N 4
    X1 20
    X2 32
    MSG point-26
    N 1008
    X1 2028
    X2 2040
    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint T1_N on view or table T1
    -At trigger CHECK_1
    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint T1_CX on view or table T1
    -At trigger CHECK_3
    MSG point-27
    N 1
    X 100
    CN 1
    CX 100
    MSG point-27
    N 2
    X 200
    CN 2
    CX 200
    MSG point-27
    N 3
    X 300
    CN 3
    CX 300
    MSG point-27
    N 4
    X 40
    CN 4
    CX 40
    MSG point-27
    N 1008
    X 88
    CN 1008
    CX 88
    MSG point-27
    N 5
    X 5
    CN 5
    CX 5
    Statement failed, SQLSTATE = 23000
    validation error for column T1.DC1, value 10
    Statement failed, SQLSTATE = 23000
    validation error for column T1.DC2, value 10
    MSG point-28
    N 1
    X 100
    CN 1
    CX 100
    DC1 <null>
    DC2 <null>
    MSG point-28
    N 2
    X 200
    CN 2
    CX 200
    DC1 <null>
    DC2 <null>
    MSG point-28
    N 3
    X 300
    CN 3
    CX 300
    DC1 <null>
    DC2 <null>
    MSG point-28
    N 4
    X 40
    CN 4
    CX 40
    DC1 <null>
    DC2 <null>
    MSG point-28
    N 1008
    X 88
    CN 1008
    CX 88
    DC1 <null>
    DC2 <null>
    MSG point-28
    N 5
    X 5
    CN 5
    CX 5
    DC1 <null>
    DC2 <null>
    MSG point-28
    N 6
    X <null>
    CN 6
    CX <null>
    DC1 6
    DC2 <null>
    MSG point-28
    N 7
    X <null>
    CN 7
    CX <null>
    DC1 <null>
    DC2 7
    MSG point-29
    BC -1
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero. The code attempted to divide an integer value by an integer divisor of zero.
    INPUT message field count: 3
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    : name: alias:
    : table: schema: owner:
    02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    : name: alias:
    : table: schema: owner:
    03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    : name: alias:
    : table: schema: owner:
    OUTPUT message field count: 2
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 8 charset: 0 NONE
    : name: CONSTANT alias: MSG
    : table: schema: owner:
    02: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    : name: COALESCE alias: COALESCE
    : table: schema: owner:
    Statement failed, SQLSTATE = 07002
    Dynamic SQL Error
    -SQLDA error
    -No SQLDA for input values provided
"""

@pytest.mark.version('>=3.0.8')
def test_2(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

