#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Check interacting with various DB objects.
DESCRIPTION: Provided by red-soft. Original file name: "unlist.test_with_objs.py"
NOTES:
    [09.04.2025] pzotov
    Checked on 6.0.0.722
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create view test_view as
    select * from unlist('1,2,3,4') as a(b)
    ;
    select b as test_01 from test_view;

    create view test_view_1 as select * from unlist('1,2,3,4') as a(test_02)
    ;
    
    select v.test_02 as test_02_a from test_view_1 v;

    select test_02 as test_02_b from test_view_1;

    recreate table test_table (id_01 int);
    insert into test_table (id_01)
    select * from unlist('1,2,3,4' returning int) as a
    ;

    select * from test_table;

    recreate table test_table_2 (id_02a int, id_02b int);
    insert into test_table_2 (id_02a, id_02b)
    select *
    from unlist('1,2,3,4' returning int) as a_1(b_1) 
    join unlist('1,2,3,4' returning int) as a_2(b_2) on b_1=b_2
    ;
    select * from test_table_2;
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = """
    TEST_01 1
    TEST_01 2
    TEST_01 3
    TEST_01 4
    TEST_02_A 1
    TEST_02_A 2
    TEST_02_A 3
    TEST_02_A 4
    TEST_02_B 1
    TEST_02_B 2
    TEST_02_B 3
    TEST_02_B 4
    ID_01 1
    ID_01 2
    ID_01 3
    ID_01 4
    ID_02A 1
    ID_02B 1
    ID_02A 2
    ID_02B 2
    ID_02A 3
    ID_02B 3
    ID_02A 4
    ID_02B 4
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
