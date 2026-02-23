#coding:utf-8

"""
ID:          table.alter-11
TITLE:       ALTER TABLE - DROP CONSTRAINT - UNIQUE
DESCRIPTION:
FBTEST:      functional.table.alter.11
NOTES:
    [06.10.2023] pzotov
    Removed SHOW command. It is enough to check that we can add duplicate values in the table w/o UNQ.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set bail on;
    set list on;
    create table test(x int, y int, constraint test_unq unique(x,y) );
    alter table test drop constraint test_unq;
    commit;
    insert into test(x, y) values(1234, 4321);
    insert into test(x, y) values(1234, 4321);
    insert into test(x, y) values(null, null);
    insert into test(x, y) values(null, null);
    commit;
    -- this must show records with duplicates in (x,y):
    select * from test order by x,y;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               <null>
    Y                               <null>
    X                               <null>
    Y                               <null>
    X                               1234
    Y                               4321
    X                               1234
    Y                               4321
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
