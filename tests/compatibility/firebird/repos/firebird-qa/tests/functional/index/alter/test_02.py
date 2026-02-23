#coding:utf-8

"""
ID:          index.alter-02
TITLE:       ALTER INDEX
DESCRIPTION:
FBTEST:      functional.index.alter.02
"""

import pytest
from firebird.qa import *

init_script = """
    create table test_active_state_toggle( a integer);
    commit;
    create index test_active_state_toggle_idx on test_active_state_toggle(a);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    alter index test_active_state_toggle_idx inactive;
    alter index test_active_state_toggle_idx active;
    commit;
    set list on;
    select rdb$index_name, rdb$index_inactive
    from rdb$indices
    where rdb$index_name=upper('test_active_state_toggle_idx');
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$INDEX_NAME                  TEST_ACTIVE_STATE_TOGGLE_IDX
    RDB$INDEX_INACTIVE              0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
