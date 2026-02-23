#coding:utf-8

"""
ID:          dml.update-or-insert-01
FBTEST:      functional.dml.update_or_insert.01
TITLE:       UPDATE OR INSERT
DESCRIPTION: Simple UPDATE OR INSERT
"""

import pytest
from firebird.qa import *

db = db_factory(init="CREATE TABLE TMPTEST( id INTEGER , name VARCHAR(20) , PRIMARY KEY(id));")

test_script = """UPDATE OR INSERT INTO TMPTEST(id, name) VALUES (1,'bob' );
select name from TMPTEST where id =1;
UPDATE OR INSERT INTO TMPTEST(id, name) VALUES (1,'ivan' );
select name from TMPTEST where id =1;"""

act = isql_act('db', test_script)

expected_stdout = """
NAME
====================
bob


NAME
====================
ivan
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
