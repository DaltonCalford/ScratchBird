#coding:utf-8

"""
ID:          issue-5971
ISSUE:       5971
TITLE:       Store precision of DECFLOAT in RDB$FIELDS
DESCRIPTION:
JIRA:        CORE-5705
FBTEST:      bugs.core_5705
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    create domain dm_df16 as decfloat(16);
    create domain dm_df34 as decfloat(34);
    commit;
    select rdb$field_name, rdb$field_precision
    from rdb$fields
    where rdb$field_name in (upper('dm_df16'), upper('dm_df34'))
    order by 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FIELD_NAME                  DM_DF16
    RDB$FIELD_PRECISION             16

    RDB$FIELD_NAME                  DM_DF34
    RDB$FIELD_PRECISION             34

    Records affected: 2
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

