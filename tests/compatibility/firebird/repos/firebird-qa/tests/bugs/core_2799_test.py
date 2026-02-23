#coding:utf-8

"""
ID:          issue-3189
ISSUE:       3189
TITLE:       Changing sort directon in delete statement cause deletion of all records in table
DESCRIPTION:
JIRA:        CORE-2799
FBTEST:      bugs.core_2799
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test
    (
        id integer not null primary key,
        kod varchar(5)
    );
    commit;

    insert into test(id, kod) values(1, 'abc');
    insert into test(id, kod) values(2, 'abc');
    commit;

    -- now we have 2 rows in table
    -- and delete in ___ascending___ oreder

    set count on;
    --set echo on;

    delete from test t
    where exists(select * from test t2 where t2.id<>t.id and t2.kod=t.kod)
    order by t.id asc;
    -- 2.5: one row affected
    -- 3.0: TWO rows must be affected.
    commit;


    select * from test;
    -- 2.5: one row selected id=2 kod='abc'
    -- 3.0: NO rows should be selected here.

    set count off;
    delete from test;
    commit;
    insert into test(id, kod) values(1, 'abc');
    insert into test(id, kod) values(2, 'abc');
    commit;
    set count on;

    -- now we have 2 rows in table
    -- and delete in ___descending___ oreder

    delete from test t
    where exists(select * from test t2 where t2.id<>t.id and t2.kod=t.kod)
    order by t.id desc;
    -- 2.5: two rows affected.
    -- 3.0: TWO rows must be affected.
    commit;

    select * from test;
    -- 2.5: empty result set.
    -- 3.0: NO rows should be selected here.

"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 2
    Records affected: 0
    Records affected: 2
    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

