#coding:utf-8

"""
ID:          trigger.alter-10
TITLE:       ALTER TRIGGER - AS
DESCRIPTION:
FBTEST:      functional.trigger.alter.10
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                   text VARCHAR(32));
SET TERM ^;
CREATE TRIGGER tg FOR test BEFORE INSERT POSITION 1
AS
BEGIN
  new.text=new.text||'tg1 ';
END ^
SET TERM ;^
commit;
"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;
ALTER TRIGGER tg AS
BEGIN
  new.text='altered trigger';
END ^

SET TERM ;^
SHOW TRIGGER tg;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*', ''), ('\\=.*', ''), ('Trigger text.*', '')])

expected_stdout = """Triggers on Table TEST:
TG, Sequence: 1, Type: BEFORE INSERT, Active
AS
BEGIN
  new.text='altered trigger';
END
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"""

@pytest.mark.skip("Covered by 'test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
