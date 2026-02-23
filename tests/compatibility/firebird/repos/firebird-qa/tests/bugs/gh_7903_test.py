#coding:utf-8

"""
ID:          issue-7903
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7903
TITLE:       Unexpected Results when Using CASE-WHEN with LEFT JOIN
DESCRIPTION:
NOTES:
    [04.12.2023] pzotov
    Confirmed bug on 6.0.0.139
    Checked on 6.0.0.169, 5.0.0.1292
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t0(id int, a int);
    recreate table t1(id int, b int);

    insert into t0(id) values (1);

    set count on;
    set list on;

    select 'case-1a' as msg, t0.*, t1.* from t0 natural left join t1 where case 1 when t1.id then false else true end;
    select 'case-1b' as msg, t0.*, t1.* from t0 natural left join t1 where case t1.id when 1 then false else true end;

    select 'case-2a' as msg, t0.*, t1.* from t0 left join t1 using(id) where case 1 when t1.id then false else true end;
    select 'case-2b' as msg, t0.*, t1.* from t0 left join t1 using(id) where case t1.id when 1 then false else true end;

    select 'case-3a' as msg, t0.*, t1.* from t0 left join t1 on t0.id = t1.id where case 1 when t1.id then false else true end;
    select 'case-3b' as msg, t0.*, t1.* from t0 left join t1 on t0.id = t1.id where case t1.id when 1 then false else true end;

"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             case-1a
    ID                              1
    A                               <null>
    ID                              <null>
    B                               <null>
    Records affected: 1

    MSG                             case-1b
    ID                              1
    A                               <null>
    ID                              <null>
    B                               <null>
    Records affected: 1

    MSG                             case-2a
    ID                              1
    A                               <null>
    ID                              <null>
    B                               <null>
    Records affected: 1

    MSG                             case-2b
    ID                              1
    A                               <null>
    ID                              <null>
    B                               <null>
    Records affected: 1

    MSG                             case-3a
    ID                              1
    A                               <null>
    ID                              <null>
    B                               <null>
    Records affected: 1

    MSG                             case-3b
    ID                              1
    A                               <null>
    ID                              <null>
    B                               <null>
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
