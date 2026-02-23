#coding:utf-8

"""
ID:          view.create-01
TITLE:       CREATE VIEW
DESCRIPTION:
FBTEST:      functional.view.create.01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test(id int);
    commit;
    create view v_test as select * from test;

    insert into test(id) values(1);
    select id from v_test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
