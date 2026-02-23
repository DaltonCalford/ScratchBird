#coding:utf-8

"""
ID:          domain.alter-02
FBTEST:      functional.domain.alter.02
TITLE:       ALTER DOMAIN - DROP DEFAULT
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    create domain test varchar(63) default 'test string';
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    alter domain test drop default;
    commit;
    set list on;
    select rdb$field_name, rdb$default_source
    from rdb$fields
    where rdb$field_name = upper('test');
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FIELD_NAME                  TEST
    RDB$DEFAULT_SOURCE              <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
