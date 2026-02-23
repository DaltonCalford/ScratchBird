#coding:utf-8

"""
ID:          domain.alter-01
FBTEST:      functional.domain.alter.01
TITLE:       ALTER DOMAIN - SET DEFAULT
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    create domain test varchar(63);
  """

db = db_factory(init=init_script)

test_script = """
    alter domain test set default 'test string';
    commit;
    set list on;
    set blob all;
    select rdb$field_name, rdb$default_source
    from rdb$fields where rdb$field_name=upper('test');
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$DEFAULT_SOURCE.*', '')])

expected_stdout = """
    RDB$FIELD_NAME                  TEST
    RDB$DEFAULT_SOURCE              2:1e1
    default 'test string'
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
