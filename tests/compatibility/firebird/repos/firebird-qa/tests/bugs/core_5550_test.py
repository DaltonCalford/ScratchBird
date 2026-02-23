#coding:utf-8

"""
ID:          issue-5818
ISSUE:       5818
TITLE:       Computed decimal field in a view has wrong RDB$FIELD_PRECISION
DESCRIPTION:
JIRA:        CORE-5550
FBTEST:      bugs.core_5550
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_test(f) as
    select cast(null as decimal(10,2))
    from rdb$database;
    commit;

    set list on;
    set count on;

    select
        cast(rf.rdb$field_name as varchar(80)) rf_field_name,
        ff.rdb$field_precision ff_field_prec,
        ff.rdb$field_scale ff_field_scale
    from rdb$relation_fields rf
    join rdb$fields ff on rf.rdb$field_source =  ff.rdb$field_name
    where rf.rdb$relation_name = upper('v_test');

"""

act = isql_act('db', test_script)

expected_stdout = """
    RF_FIELD_NAME                   F
    FF_FIELD_PREC                   18
    FF_FIELD_SCALE                  -2
    Records affected: 1
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

