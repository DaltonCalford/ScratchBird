#coding:utf-8

"""
ID:          issue-991
ISSUE:       991
TITLE:        Grouping on derived fields processing NULL data kills IB
DESCRIPTION:
JIRA:        CORE-629
FBTEST:      bugs.core_0629
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create or alter view v_test as select 1 id from rdb$database;
    commit;

    recreate table test(
        id integer not null,
        dt_beg date,
        dt_end date,
        constraint pk_test primary key (id)
    );
    commit;

    create or alter view v_test as
    select id, extract(year from dt_beg) - extract(year from dt_end) dy
    from test;
    commit;

    insert into test values(1, '01.01.2015', null);
    insert into test values(2, '01.01.2015', '01.01.2015');
    insert into test values(3, null, null);
    insert into test values(4, null, null);
    insert into test values(5, '01.01.2015', '31.12.2014');
    commit;

    select dy from v_test group by dy;
    commit;

    -------------------------------------------

    create or alter view v_test as select 1 id from rdb$database;
    commit;
    recreate table test
    (
      a integer,
      b date,
      c computed by (extract(day from b)-extract(day from b))
    );
    commit;
    insert into test(a, b) values(1, DATE '2015-05-24');
    insert into test(a, b) values(1, null);
    commit;
    select c from test group by c;
    commit;

    create or alter view v_test as select b-b as dd from test;
    commit;
    select dd from v_test group by dd;
    commit;

    create or alter view v_test as select b-0 as dd from test;
    select dd from v_test group by dd;

    create or alter view v_test
    as select cast(b as timestamp) as dt  from test;
    select dt from v_test group by dt;

    ------------

    create or alter view v_test as select 1 id from rdb$database;
    commit;
    recreate table test(a int, b time, c computed by(cast(b as time)));
    commit;

    insert into test(a, b) values(1, '15:00:29.191');
    insert into test(a, b) values(1, null);
    commit;

    select c from test group by c;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DY                              <null>
    DY                              0
    DY                              1

    C                               <null>
    C                               0

    DD                              <null>
    DD                              0

    DD                              <null>
    DD                              2015-05-24

    DT                              <null>
    DT                              2015-05-24 00:00:00.0000

    C                               <null>
    C                               15:00:29.1910
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

