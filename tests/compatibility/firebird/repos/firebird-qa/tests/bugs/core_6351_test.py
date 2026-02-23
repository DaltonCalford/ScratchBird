#coding:utf-8

"""
ID:          issue-6592
ISSUE:       6592
TITLE:       Computed field could be wrongly evaluated as NULL
DESCRIPTION:
JIRA:        CORE-6351
FBTEST:      bugs.core_6351
"""

import pytest
from firebird.qa import *

init_script = """
    create table t1
    (
      id int,
      f1 computed by ('abcd'),
      f2 computed by ('xyz')
    );
    commit;

    set term ^;
    create or alter procedure p_t1 (id int)
      returns (val varchar(32))
    as
    begin
      val = 'unknown';

      select f2 from t1 where id = :id
        into :val;

      suspend;
    end^
    set term ;^
    commit;

    alter table t1
      alter f1 computed by ((select val from p_t1(id)));

    alter table t1
      alter f2 computed by ('id = ' || id);
    commit;

    insert into t1 values (1);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select 'case-1' as msg, p.val from p_t1(1) p;
    select t.* from t1 t;
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';

    select t.* from t1 t;
    select 'case-2' as msg, p.val from p_t1(1) p;
    exit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             case-1
    VAL                             id = 1
    ID                              1
    F1                              id = 1
    F2                              id = 1

    ID                              1
    F1                              id = 1
    F2                              id = 1
    MSG                             case-2
    VAL                             id = 1
"""

@pytest.mark.version('>=3.0.7')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
