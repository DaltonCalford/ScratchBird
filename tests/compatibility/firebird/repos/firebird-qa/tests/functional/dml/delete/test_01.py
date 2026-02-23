#coding:utf-8

"""
ID:          dml.delete-01
FBTEST:      functional.dml.delete.01
TITLE:       DELETE
DESCRIPTION:
"""

import pytest
from firebird.qa import db_factory, isql_act, Action

init_script = """CREATE TABLE tb(id INT);
INSERT INTO tb VALUES(10);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """DELETE FROM tb;
SELECT * FROM tb;"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
