#coding:utf-8

"""
ID:          issue-5811
ISSUE:       5811
TITLE:       Restore of pre ODS 11.1 database can leave RDB$RELATION_TYPE null
DESCRIPTION:
JIRA:        CORE-5543
FBTEST:      bugs.core_4473
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core_4473-ods10_1.fbk')

test_script = """
    -- Source DB was created under FB 1.5.6 (ODS 10.1) and contains following objects:
    -- create table test_t(x int);
    -- create view test_v(x) as select x from test_t;
    -- Value of rdb$relations.rdb$relation_type for these objects must be zero rather than null.

    set list on;
    select rdb$relation_type
    from rdb$relations
    where
        rdb$relation_name starting with upper('test')
        and rdb$system_flag is distinct from 1
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$RELATION_TYPE               0
    RDB$RELATION_TYPE               0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

