#coding:utf-8

"""
ID:          trigger.create-01
TITLE:       CREATE TRIGGER
DESCRIPTION:
FBTEST:      functional.trigger.create.01
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE tb(id INT);
commit;
"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;
/* Tested command: */
CREATE TRIGGER test FOR tb BEFORE INSERT AS
BEGIN
  new.id=1;
END^
SET TERM ;^
SHOW TRIGGER test;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*', ''), ('\\=.*', ''), ('Trigger text.*', '')])

expected_stdout = """Triggers on Table TB:
TEST, Sequence: 0, Type: BEFORE INSERT, Active
AS
BEGIN
  new.id=1;
END
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"""

@pytest.mark.skip("Covered by 'test_create_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
