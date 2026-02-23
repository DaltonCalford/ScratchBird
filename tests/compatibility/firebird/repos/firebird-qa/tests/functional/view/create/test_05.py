#coding:utf-8

"""
ID:          view.create-05
TITLE:       CREATE VIEW
DESCRIPTION:
FBTEST:      functional.view.create.05
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(3);
INSERT INTO tb VALUES(10);
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """CREATE VIEW test (id,num) AS SELECT id,5 FROM tb;
SELECT * FROM test;
"""

act = isql_act('db', test_script)

expected_stdout = """          ID          NUM
============ ============

           3            5
10            5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
