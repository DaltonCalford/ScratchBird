#coding:utf-8

"""
ID:          issue-361
ISSUE:       361
TITLE:       Error "no current record for fetch operation" on view select
DESCRIPTION:
NOTES:
[24.01.2019] Added separate code for running on FB 4.0+.
             UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
             Functions div, frac, dow, sdow, getExactTimestampUTC and isLeapYear got safe
             replacement in UDR library "udf_compat", see it in folder: ../plugins/udr/
JIRA:        CORE-37
FBTEST:      bugs.core_0037
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script_1 = """
    set bail on;
    recreate view v1 as select 1 k from rdb$database;
    recreate view v2 as select 1 k from rdb$database;
    commit;

    set term ^;
    execute block as
    begin
        execute statement 'drop function udf_frac';
    when any do
        begin end
    end
    ^
    set term ;^
    commit;

    declare external function udf_frac
        double precision
        returns double precision by value
        entry_point 'IB_UDF_frac' module_name 'ib_udf';

    commit;

    recreate table t1
    (
      t1f1 integer not null primary key,
      t1f2 varchar(30)
    );

    recreate table t2
    (
      t2f1 integer not null primary key,
      t2f2 integer not null,
      t2f3 varchar(30)
    );

    create index t2f2_ndx on t2(t2f2);

    recreate view v1 as select * from t1 where udf_frac( mod(t1f1,100) / 100.000) > 0.03;
    recreate view v2 as select * from t2 where udf_frac( mod(t2f1,100) / 100.000) > 0.03;

    insert into t1(t1f1, t1f2) values (1, '1');
    insert into t1(t1f1, t1f2) values (2, '2');
    insert into t1(t1f1, t1f2) values (3, '3');
    insert into t1(t1f1, t1f2) values (104, '104');
    insert into t1(t1f1, t1f2) values (105, '205');
    insert into t1(t1f1, t1f2) values (106, '106');

    insert into t2(t2f1, t2f2, t2f3) values (1, 1, '1');
    insert into t2(t2f1, t2f2, t2f3) values (2, 2, '2');
    insert into t2(t2f1, t2f2, t2f3) values (3, 3, '3');
    insert into t2(t2f1, t2f2, t2f3) values (104, 104, '104');
    insert into t2(t2f1, t2f2, t2f3) values (105, 105, '105');
    insert into t2(t2f1, t2f2, t2f3) values (106, 106, '106');

    set count on;

    select x.*, y.*, udf_frac( mod(t2f1,100) / 100.000)
    from v1 x, v2 y
    where x.t1f1 = y.t2f2
    and udf_frac( mod(t2f1,100) / 100.000) < 0.03
    ;
"""

act_1 = isql_act('db', test_script_1)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0
test_script_2 = """
    set bail on;

    -- See declaration sample in plugins\\udr\\UdfBackwardCompatibility.sql:

    create function UDR40_frac (
        val double precision
    ) returns double precision
    external name 'udf_compat!UC_frac'
    engine udr;

    recreate view v1 as select 1 k from rdb$database;
    recreate view v2 as select 1 k from rdb$database;
    commit;

    recreate table t1
    (
      t1f1 integer not null primary key,
      t1f2 varchar(30)
    );

    recreate table t2
    (
      t2f1 integer not null primary key,
      t2f2 integer not null,
      t2f3 varchar(30)
    );

    create index t2f2_ndx on t2(t2f2);

    recreate view v1 as select * from t1 where UDR40_frac( mod(t1f1,100) / 100.000) > 0.03;
    recreate view v2 as select * from t2 where UDR40_frac( mod(t2f1,100) / 100.000) > 0.03;

    insert into t1(t1f1, t1f2) values (1, '1');
    insert into t1(t1f1, t1f2) values (2, '2');
    insert into t1(t1f1, t1f2) values (3, '3');
    insert into t1(t1f1, t1f2) values (104, '104');
    insert into t1(t1f1, t1f2) values (105, '205');
    insert into t1(t1f1, t1f2) values (106, '106');

    insert into t2(t2f1, t2f2, t2f3) values (1, 1, '1');
    insert into t2(t2f1, t2f2, t2f3) values (2, 2, '2');
    insert into t2(t2f1, t2f2, t2f3) values (3, 3, '3');
    insert into t2(t2f1, t2f2, t2f3) values (104, 104, '104');
    insert into t2(t2f1, t2f2, t2f3) values (105, 105, '105');
    insert into t2(t2f1, t2f2, t2f3) values (106, 106, '106');

    set count on;

    select x.*, y.*, UDR40_frac( mod(t2f1,100) / 100.000)
    from v1 x, v2 y
    where x.t1f1 = y.t2f2
    and UDR40_frac( mod(t2f1,100) / 100.000) < 0.03
    ;
"""

act_2 = isql_act('db', test_script_2)

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout

