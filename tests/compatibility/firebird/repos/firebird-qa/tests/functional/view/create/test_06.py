#coding:utf-8

"""
ID:          view.create-06
TITLE:       CREATE VIEW - updateable
DESCRIPTION:
FBTEST:      functional.view.create.06
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
commit;
"""

db = db_factory(init=init_script)

test_script = """CREATE VIEW test (id) AS SELECT id FROM tb;
INSERT INTO test VALUES(2);
COMMIT;
SELECT * FROM test;
"""

act = isql_act('db', test_script)

expected_stdout = """          ID
============

2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
