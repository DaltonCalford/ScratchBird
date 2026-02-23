#coding:utf-8

"""
ID:          issue-5163
ISSUE:       5163
TITLE:       Server crash when preparing a query with PLAN clause at some CTE
DESCRIPTION:
JIRA:        CORE-4867
FBTEST:      bugs.core_4867
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Tested on:
    -- LI-V2.5.5.26910 after commit http://sourceforge.net/p/firebird/code/61955
    -- LI-V3.0.0.31924

    recreate table table1 (
      field1 integer not null
    );

    recreate table table2 (
      field1 integer not null,
      date1 date not null
    );

    insert into table1 values(1);
    insert into table2 values(1, date '30.06.2015');
    insert into table2 values(1, date '31.05.2015');
    insert into table2 values(1, date '31.07.2015');

    insert into table1 values(2);
    insert into table2 values(2, date '31.03.2012');
    insert into table2 values(2, date '29.05.2012');
    insert into table2 values(2, date '31.01.2012');
    commit;

    alter table table1 add constraint pk_table1 primary key (field1);
    alter table table2 add constraint pk_table2 primary key (field1, date1)
      using descending index pk_table2;
    create index idx_table2 on table2 (field1, date1);
    commit;

    set list on;
    with aa
    as (select t1.field1,
               (select first 1 t2.date1
                  from table2 t2
                 where t2.field1 = t1.field1
                plan(t2 index(idx_table2))
                order by t2.field1 asc, t2.date1 asc) as date1
          from table1 t1)

    select date1, count('x')
      from aa
    group by 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DATE1                           2012-01-31
    COUNT                           1

    DATE1                           2015-05-31
    COUNT                           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

