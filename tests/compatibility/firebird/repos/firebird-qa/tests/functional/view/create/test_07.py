#coding:utf-8

"""
ID:          view.create-07
TITLE:       CREATE VIEW - updateable WITH CHECK OPTION
DESCRIPTION:
FBTEST:      functional.view.create.07
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
"""

db = db_factory(init=init_script)

test_script = """CREATE VIEW test (id) AS SELECT id FROM tb WHERE id<10 WITH CHECK OPTION;
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
