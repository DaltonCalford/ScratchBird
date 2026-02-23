#coding:utf-8

"""
ID:          issue-3518
ISSUE:       3518
TITLE:       The last column in a view is returning as a null value even when it's not
DESCRIPTION:
  NB: new datatype in FB 4.0 was introduces: numeric(38,0).
  It leads to additional ident of values when we show them in form "SET LIST ON",
  so we have to ignore all internal spaces - see added 'substitution' section below.
JIRA:        CORE-3141
FBTEST:      bugs.core_3141
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create table test(
       id integer,
       datum date,
       sfield varchar (10),
       num1 numeric (10,2),
       num2 numeric (10,2),
       primary key (id)
    );
    commit;

    insert into test values (1, '2013-06-04', 'A', 10, 0);
    insert into test values (2, '2013-06-04', 'B', 0, 10);
    insert into test values (3, '2013-06-04', 'A', 20, 0);
    insert into test values (4, '2013-06-04', 'B', 0, 20);

    create view v_test1 as
    select
        t.datum,
        iif ((select sum(num1-num2) from test where sfield = 'A' and datum <= t.datum) is null, 0, (select sum(num1-num2) from test where sfield = 'A' and datum <= t.datum)) A,
        iif ((select sum(num1-num2) from test where sfield = 'A' and datum <= t.datum) is null, 0, (select sum(num1-num2) from test where sfield = 'B' and datum <= t.datum)) B,
        iif ((select sum(num1-num2) from test where sfield = 'A' and datum <= t.datum) is null, 0, (select sum(num1-num2) from test where sfield = 'A' and datum <= t.datum)) A1,
        iif ((select sum(num1-num2) from test where sfield = 'A' and datum <= t.datum) is null, 0, (select sum(num1-num2) from test where sfield = 'B' and datum <= t.datum)) B1
    from test t
    group by t.datum;

    create view v_test2 as
    select
        t.datum,
        0 as A,
        0 as B,
        0 as A1,
        10 as B1
    from test t
    group by t.datum;
    commit;

    select 'test_1a' as msg, v.* from v_test1 v;
    select 'test_1b' as msg, v.* from v_test2 v;
    commit;

    drop view v_test1;
    drop view v_test2;
    drop table test;

    ---------------------------------------------

    -- From core-3159:
    recreate view v_test1 as select 1 i from rdb$database;
    commit;

    recreate table test1 (
      id integer default 0 not null,
      variance numeric (10,2),
      anumber integer default 0,
      primary key (id)
    );
    recreate table test2 (
      id integer default 0 not null,
      primary key (id)
    );

    recreate view v_test1 (databasename, testvalue, placer) as
    select id, (select sum (variance) from test1 where anumber = 10), 10
    from test2
    group by id;
    commit;

    insert into test1 (id, variance, anumber)
    values (1, 100.00, 10);

    insert into test1 (id, variance, anumber)
    values (2, 150.00, 10);

    insert into test1 (id, variance, anumber)
    values (3, 150.00, 12);

    commit;

    insert into test2 (id) values (10);
    insert into test2 (id) values (12);

    commit;

    select 'test_2' as msg, v.* from v_test1 v; --this one doesn't
    commit;
    drop view v_test1;
    drop table test1;
    drop table test2;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MSG                             test_1a
    DATUM                           2013-06-04
    A                               30.00
    B                               -30.00
    A1                              30.00
    B1                              -30.00
    MSG                             test_1b
    DATUM                           2013-06-04
    A                               0
    B                               0
    A1                              0
    B1                              10
    MSG                             test_2
    DATABASENAME                    10
    TESTVALUE                       250.00
    PLACER                          10
    MSG                             test_2
    DATABASENAME                    12
    TESTVALUE                       250.00
    PLACER                          10
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

