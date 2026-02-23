#coding:utf-8

"""
ID:          trigger.alter-13
TITLE:       ALTER TRIGGER - AS
DESCRIPTION: Try use old prefix in INSERT trigger
FBTEST:      functional.trigger.alter.23
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE, text VARCHAR(32));
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

test_script = """
    SET TERM ^;
    ALTER TRIGGER tg AS
    BEGIN
    old.text='altered trigger';
    END ^
    SET TERM ;^
"""

act = isql_act('db', test_script, substitutions=[('At line.*', 'At line')])

expected_stderr = """
    Statement failed, SQLSTATE = 42S22
    unsuccessful metadata update
    -ALTER TRIGGER TG failed
    -Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -OLD.TEXT
    -At line 3, column 3
"""

@pytest.mark.skip("Covered by 'test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
