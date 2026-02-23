#coding:utf-8

"""
ID:          trigger.create-08
TITLE:       CREATE TRIGGER AFTER DELETE POSITION 12
DESCRIPTION:
FBTEST:      functional.trigger.create.08
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;
CREATE TRIGGER test FOR tb AFTER DELETE POSITION 12 AS
BEGIN
END^
SET TERM ;^
SHOW TRIGGER test;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*', ''), ('\\=.*', ''), ('Trigger text.*', '')])

expected_stdout = """Triggers on Table TB:
TEST, Sequence: 12, Type: AFTER DELETE, Active
AS
BEGIN
END
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"""

@pytest.mark.skip("Covered by 'test_create_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
