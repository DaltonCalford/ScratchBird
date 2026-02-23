#coding:utf-8

"""
ID:          index.alter-01
TITLE:       ALTER INDEX - INACTIVE
DESCRIPTION:
FBTEST:      functional.index.alter.01
"""

import pytest
from firebird.qa import *

init_script = """
    create table test( a integer);
    create index test_idx on test(a);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    alter index test_idx inactive;
    commit;
    set list on;
    select
        rdb$index_name as idx_name,
        rdb$index_inactive as is_inactive
    from rdb$indices
    where rdb$index_name=upper('test_idx');
"""

act = isql_act('db', test_script)

expected_stdout = """
    IDX_NAME                        TEST_IDX
    IS_INACTIVE                     1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
