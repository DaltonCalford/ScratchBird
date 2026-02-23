#coding:utf-8

"""
ID:          dml.delete-02
FBTEST:      functional.dml.delete.02
TITLE:       DELETE with WHERE
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(10);
INSERT INTO tb VALUES(20);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """DELETE FROM tb WHERE id>10;
SELECT * FROM tb;"""

act = isql_act('db', test_script)

expected_stdout = """
ID
============

          10
10
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
