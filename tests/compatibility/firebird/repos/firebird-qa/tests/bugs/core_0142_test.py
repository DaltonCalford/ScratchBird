#coding:utf-8

"""
ID:          issue-469
ISSUE:       469
TITLE:       Index breaks = ANY result
DESCRIPTION:
JIRA:        CORE-142
FBTEST:      bugs.core_0142
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed wrong output on WI-V1.5.6.5026.
    -- Since 2.0.0 works fine.

    recreate view v_test(k) as select 1 k from rdb$database;
    commit;

    recreate table customers (
        cnum integer,
        cname char(10),
        city char(10),
        rating integer,
        snum integer
    );

    recreate view v_test(cnum, cname, city, rating, snum) as
    select *
    from customers c
    where not c.rating = any
    (select r.rating
    from customers r
    where r.city = 'san jose');
    commit;

    insert into customers values (2001, 'hoffman', 'london', 100, 1001);
    insert into customers values (2002, 'giovanni', 'rome', 200, 1003);
    insert into customers values (2003, 'lui', 'san jose', 200, 1002);
    insert into customers values (2004, 'grass', 'berlin', 300, 1002);
    insert into customers values (2006, 'clemens', 'london', null, 1001);
    insert into customers values (2008, 'cisneros', 'san jose', 300, 1007);
    insert into customers values (2007, 'pereira', 'rome', 100, 1004);
    commit;

    set list on;

    select * from v_test order by cnum;
    commit;

    create index byrating on customers (rating);
    commit;

    select * from v_test order by cnum;
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    CNUM                            2001
    CNAME                           hoffman
    CITY                            london
    RATING                          100
    SNUM                            1001

    CNUM                            2007
    CNAME                           pereira
    CITY                            rome
    RATING                          100
    SNUM                            1004

    CNUM                            2001
    CNAME                           hoffman
    CITY                            london
    RATING                          100
    SNUM                            1001

    CNUM                            2007
    CNAME                           pereira
    CITY                            rome
    RATING                          100
    SNUM                            1004
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

