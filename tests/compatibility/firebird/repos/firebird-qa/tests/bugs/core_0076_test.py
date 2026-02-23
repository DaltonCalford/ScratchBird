#coding:utf-8

"""
ID:          issue-400
ISSUE:       400
TITLE:       Invalid ROW_COUNT variable value after DELETE
DESCRIPTION:
JIRA:        CORE-76
FBTEST:      bugs.core_0076
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure test_del as begin end;

    recreate table test (
        a integer not null,
        constraint test_pk primary key (a)
    );
    insert into test (a) values (1);
    insert into test (a) values (2);
    insert into test (a) values (3);
    insert into test (a) values (4);
    insert into test (a) values (5);
    insert into test (a) values (6);
    insert into test (a) values (7);
    insert into test (a) values (8);
    insert into test (a) values (9);
    insert into test (a) values (10);
    commit;

    set list on;
    select count(*) as cnt from test where a between 4 and 7;

    set term ^;
    create or alter procedure test_del (l integer, r integer) returns (rc integer) as
    begin
        delete from test where a between :l and :r;
        rc = row_count;
        suspend;
    end
    ^
    set term ;^
    execute procedure test_del (4, 7);
    select * from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             4
    RC                              4
    A                               1
    A                               2
    A                               3
    A                               8
    A                               9
    A                               10
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

