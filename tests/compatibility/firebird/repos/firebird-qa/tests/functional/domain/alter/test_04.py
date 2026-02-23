#coding:utf-8

"""
ID:          domain.alter-04
FBTEST:      functional.domain.alter.04
TITLE:       ALTER DOMAIN - DROP CONSTRAINT
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    create domain test varchar(63) check(value like 'te%');
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    set list on;
    alter domain test drop constraint;
    commit;
    select rdb$field_name, rdb$validation_source
    from rdb$fields
    where rdb$field_name = upper('test');
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FIELD_NAME                  TEST
    RDB$VALIDATION_SOURCE           <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
