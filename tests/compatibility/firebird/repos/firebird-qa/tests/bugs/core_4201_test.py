#coding:utf-8

"""
ID:          issue-4526
ISSUE:       4526
TITLE:       Regression: Computed field returns null value inside BI trigger
DESCRIPTION:
JIRA:        CORE-4201
FBTEST:      bugs.core_4201
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test (
        field1 integer not null,
        fld_calc computed by (field1+1),
        field2 integer
    );
    commit;

    set term ^ ;

    create trigger test_bi0 for test
    active before insert position 0
    as
    begin
      new.field2 = new.fld_calc;
      rdb$set_context('USER_TRANSACTION','NEW_FLD1', new.field1);
      rdb$set_context('USER_TRANSACTION','FLD_CALC', new.fld_calc);
      rdb$set_context('USER_TRANSACTION','NEW_FLD2', new.field2);
    end
    ^
    set term ; ^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    insert into test (field1) values (1);

    set list on;
    select
       cast(rdb$get_context('USER_TRANSACTION','NEW_FLD1') as bigint) as new_fld1
      ,cast(rdb$get_context('USER_TRANSACTION','FLD_CALC') as bigint) as fld_calc
      ,cast(rdb$get_context('USER_TRANSACTION','NEW_FLD2') as bigint) as new_fld2
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    NEW_FLD1                        1
    FLD_CALC                        2
    NEW_FLD2                        2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

