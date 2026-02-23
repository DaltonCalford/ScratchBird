#coding:utf-8

"""
ID:          trigger.alter-03
TITLE:       ALTER TRIGGER - BEFORE DELETE
DESCRIPTION:
FBTEST:      functional.trigger.alter.03
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                   text VARCHAR(32));
SET TERM ^;
CREATE TRIGGER tg FOR test AFTER UPDATE
AS
BEGIN
END ^
SET TERM ;^
commit;
"""

db = db_factory(init=init_script)

test_script = """ALTER TRIGGER tg BEFORE DELETE;
SHOW TRIGGER tg;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*', ''), ('\\=.*', ''), ('Trigger text.*', '')])

expected_stdout = """Triggers on Table TEST:
TG, Sequence: 0, Type: BEFORE DELETE, Active
AS
BEGIN
END
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"""

@pytest.mark.skip("Covered by 'test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
