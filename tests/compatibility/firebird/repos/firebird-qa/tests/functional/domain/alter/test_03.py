#coding:utf-8

"""
ID:          domain.alter-03
FBTEST:      functional.domain.alter.03
TITLE:       ALTER DOMAIN - ADD CONSTRAINT
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    create domain test varchar(63);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    alter domain test add constraint check (value like 'te%');
    commit;
    set list on;
    set blob all;
    select rdb$field_name, rdb$validation_source from rdb$fields where rdb$field_name=upper('test');
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$VALIDATION_SOURCE.*', '')])

expected_stdout = """
    RDB$FIELD_NAME                  TEST
    check (value like 'te%')
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
