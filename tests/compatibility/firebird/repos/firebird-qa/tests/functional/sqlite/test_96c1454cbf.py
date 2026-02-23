#coding:utf-8

"""
ID:          96c1454cbf
ISSUE:       https://www.sqlite.org/src/tktview/96c1454cbf
TITLE:       Incorrect result with ORDER BY DESC and LIMIT
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x int primary key using index t1_pk, y int);
    create table t2(z int);

    insert into t1(x,y) values(1,1);
    insert into t1(x,y) values(2,1);
    insert into t1(x,y) values(3,1);
    insert into t1(x,y) values(4,1);
    insert into t1(x,y) values(5,5);
    insert into t1(x,y) values(6,6);
    insert into t1(x,y) values(7,4);

    insert into t2(z) values(1);
    insert into t2(z) values(2);
    insert into t2(z) values(3);
    insert into t2(z) values(4);
    insert into t2(z) values(5);
    insert into t2(z) values(6);
    insert into t2(z) values(7);

    set count on;
    select '1' msg,x,y from t1 where x in (select z from t2) order by y desc;
    select '2' msg,x,y from t1 where x in (select z from t2) order by y desc rows 3;
    set count off;
    commit;

    --########################################################################
    -- also: https://www.sqlite.org/src/tktview/0c4df46116

    create table t3(a int, b int, c int);
    create index t3x on t3(a,b,c);
    create descending index t3c on t3(c,b,a);
    insert into t3 values(0,1,99);
    insert into t3 values(0,1,0);
    insert into t3 values(0,0,0);
    --  uncomment only if output mismatch occurs: --> set plan on;
    set count on;
    select '3' msg, t3.* from t3 where a=0 and (c=0 or c=99) order by c desc, b desc, a desc;
    select '4' msg, t3.* from t3 where a=0 and (c=0 or c=99) order by c desc, b desc, a desc rows 1;
    set count off;
"""

substitutions = [('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG 1
    X 6
    Y 6
    MSG 1
    X 5
    Y 5
    MSG 1
    X 7
    Y 4
    MSG 1
    X 1
    Y 1
    MSG 1
    X 2
    Y 1
    MSG 1
    X 3
    Y 1
    MSG 1
    X 4
    Y 1
    Records affected: 7
    MSG 2
    X 6
    Y 6
    MSG 2
    X 5
    Y 5
    MSG 2
    X 7
    Y 4
    Records affected: 3
    MSG 3
    A 0
    B 1
    C 99
    MSG 3
    A 0
    B 1
    C 0
    MSG 3
    A 0
    B 0
    C 0
    Records affected: 3
    MSG 4
    A 0
    B 1
    C 99
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
