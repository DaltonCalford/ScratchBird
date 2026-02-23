#coding:utf-8

"""
ID:          issue-4025
ISSUE:       4025
TITLE:       CREATE INDEX considers NULL and empty string being the same in compound indices
DESCRIPTION:
JIRA:        CORE-3675
FBTEST:      bugs.core_3675
NOTES:
    [05.10.2023] pzotov
    Removed SHOW command for check result because its output often changes.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate table test(
        f1 varchar(1)
       ,f2 varchar(1)
       ,f3 varchar(1)
       ,f4 varchar(1)
       ,id int primary key
       ,constraint test_unq unique(f1,f2,f3,f4)
    );
    commit;
    insert into test values('a',  'b',   'c',  'd',  1);
    insert into test values('a', null,   'c',  'd',  2);
    insert into test values('a',   '',   'c',  'd',  3);
    insert into test values('a',   'b', null,  'd',  4);
    insert into test values('a',   'b', null,   '',  5);
    insert into test values('a',   'b',  '',  null,  6);
    insert into test values('a',   'b', null, null,  7);
    insert into test values('a',  null, null, null,  8);
    insert into test values(null, null, null, null,  9);
    commit;
    select id,f1,f2,f3,f4 from test order by id;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    F1                              a
    F2                              b
    F3                              c
    F4                              d

    ID                              2
    F1                              a
    F2                              <null>
    F3                              c
    F4                              d
    
    ID                              3
    F1                              a
    F2
    F3                              c
    F4                              d
    
    ID                              4
    F1                              a
    F2                              b
    F3                              <null>
    F4                              d
    
    ID                              5
    F1                              a
    F2                              b
    F3                              <null>
    F4
    
    ID                              6
    F1                              a
    F2                              b
    F3
    F4                              <null>
    
    ID                              7
    F1                              a
    F2                              b
    F3                              <null>
    F4                              <null>
    
    ID                              8
    F1                              a
    F2                              <null>
    F3                              <null>
    F4                              <null>
    
    ID                              9
    F1                              <null>
    F2                              <null>
    F3                              <null>
    F4                              <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

