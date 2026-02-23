#coding:utf-8

"""
ID:          trigger.table.alter-07
TITLE:       ALTER TRIGGER: attempt to use "new" in AFTER INSERT
DESCRIPTION: an attempt to change the trigger event to 'AFTER' should be rejected if trigger uses 'new' context variable.
FBTEST:      functional.trigger.table.alter_07
"""

import pytest
from firebird.qa import *

init_script = """
    create table test( id int primary key);
    set term ^;
    create trigger trg_test_bu for test before update as
    begin
        new.id=1;
    end ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    ALTER TRIGGER trg_test_bu AFTER INSERT;
"""

# FB 3.x:
#     Statement failed, SQLSTATE = 42000
#     attempted update of read-only column
# FB 4.x and 5.x:
#     Statement failed, SQLSTATE = 42000
#     attempted update of read-only column TEST.ID

substitutions = [
                  ('^((?!SQLSTATE|column).)*$', '')
                  ,('-attempted', 'attempted')
                ]

act = isql_act('db', test_script, substitutions = substitutions)


expected_stderr_fb3 = """    	
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
"""

expected_stderr_fb4 = """
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.ID
"""

@pytest.mark.skip("Covered by 'functional/trigger/alter/test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    if act.is_version('>=4.0'):
        act.expected_stderr = expected_stderr_fb4
    else:
        act.expected_stderr = expected_stderr_fb3
    
    act.execute()
    
    assert act.clean_stderr == act.clean_expected_stderr
