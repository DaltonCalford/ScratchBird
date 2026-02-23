#coding:utf-8

"""
ID:          trigger.table.alter-12
TITLE:       ALTER TRIGGER - AS
DESCRIPTION:
  Try use old prefix in INSERT trigger
FBTEST:      functional.trigger.table.alter_12
"""

import pytest
from firebird.qa import *

init_script = """
    create table test(id integer not null constraint unq unique, text varchar(32));
    commit;
    set term ^;
    create trigger tg for test before insert position 1 as
    begin
        new.text=new.text||'tg1 ';
    end
    ^
    set term ;^
    commit;

"""

db = db_factory(init=init_script)

test_script = """
    -- Since WI-T3.0.0.31733 content of STDERR has been changed: source position of
    -- problematic statement is displayed now on seperate line, like this:
    -- "-At line 4, column 1"
    -- Decided to suppress this line.
    set term ^;
    alter trigger tg as
    begin
        old.text = 'altered trigger';
    end ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('At line.*', '')])

expected_stderr = """
    Statement failed, SQLSTATE = 42S22
    unsuccessful metadata update
    -ALTER TRIGGER TG failed
    -Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -OLD.TEXT
    -At line 4, column 1
"""

@pytest.mark.skip("Covered by 'functional/trigger/alter/test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
