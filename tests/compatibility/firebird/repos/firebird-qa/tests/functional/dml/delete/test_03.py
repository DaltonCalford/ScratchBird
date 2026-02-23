#coding:utf-8

"""
ID:          dml.delete-03
FBTEST:      functional.dml.delete.03
TITLE:       DELETE from VIEW
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
CREATE VIEW test (id) AS SELECT id FROM tb;
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(null);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """DELETE FROM test WHERE id=10;
SELECT * FROM tb;"""

act = isql_act('db', test_script)

expected_stdout = """
ID
============

<null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
