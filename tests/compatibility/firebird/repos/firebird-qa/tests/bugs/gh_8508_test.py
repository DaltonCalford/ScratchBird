#coding:utf-8

"""
ID:          issue-8508
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8508
TITLE:       Conversion Error with old.field in UPDATE OR INSERT
DESCRIPTION:
NOTES:
    [09.04.2025] pzotov
    Confirmed bug on 6.0.0.717; 5.0.3.1639.
    Checked on 6.0.0.722; 5.0.3.1641; 4.0.6.3194; 3.0.13.33806.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (id int primary key);
    insert into test(id) values(999);

    update or insert into test (id) values (999)
    matching (id)
    returning old.id as old_id, new.id as new_id;
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = """
    OLD_ID 999
    NEW_ID 999
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
