#coding:utf-8

"""
ID:          issue-2684
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2684
TITLE:       Internal error when select upper(<blob>) from union
DESCRIPTION:
JIRA:        CORE-2258
FBTEST:      bugs.core_2258
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select * from
    (
        select cast('123' as blob sub_type text) from rdb$database
        union all
        select cast('123' as blob sub_type text) from rdb$database
    ) as r (blob_field_id)
    ;

    select upper(blob_field) from
    (
        select cast('123' as blob sub_type text) from rdb$database
        union all
        select cast('123' as blob sub_type text) from rdb$database
    ) as r (blob_field)
    ;
"""

act = isql_act('db', test_script, substitutions = [('BLOB_FIELD_ID .*', 'BLOB_FIELD_ID'), ('UPPER.*', 'UPPER')])

expected_stdout = """
    BLOB_FIELD_ID                   0:1
    123
    BLOB_FIELD_ID                   0:3
    123
    Records affected: 2

    UPPER                           0:7
    123
    UPPER                           0:b
    123
    Records affected: 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

