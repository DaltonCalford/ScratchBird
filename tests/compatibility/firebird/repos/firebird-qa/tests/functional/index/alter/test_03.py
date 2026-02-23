#coding:utf-8

"""
ID:          index.alter-03
TITLE:       ALTER INDEX - INACTIVE UNIQUE INDEX
DESCRIPTION:
FBTEST:      functional.index.alter.03
"""

import pytest
from firebird.qa import *

init_script = """
    create table test( a integer);
    create unique index test_idx_unq on test(a);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    alter index test_idx_unq inactive;
    commit;
    set list on;
    select
        rdb$index_name as idx_name
        ,rdb$index_inactive as is_inactive
        ,r.rdb$unique_flag as is_unique
    from rdb$indices r
    where rdb$index_name=upper('test_idx_unq');
"""

act = isql_act('db', test_script)

expected_stdout = """
    IDX_NAME                        TEST_IDX_UNQ
    IS_INACTIVE                     1
    IS_UNIQUE                       1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
