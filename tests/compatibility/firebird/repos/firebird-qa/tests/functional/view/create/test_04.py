#coding:utf-8

"""
ID:          view.create-04
TITLE:       CREATE VIEW - bad number of columns
DESCRIPTION:
FBTEST:      functional.view.create.04
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
"""

db = db_factory(init=init_script)

test_script = """CREATE VIEW test (id) AS SELECT id,5 FROM tb;
SHOW VIEW test;
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 07002
unsuccessful metadata update
-CREATE VIEW TEST failed
-SQL error code = -607
-Invalid command
-number of columns does not match select list
There is no view TEST in this database
"""

@pytest.mark.skip("Covered by 'tests/functional/view/create/test_03.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
