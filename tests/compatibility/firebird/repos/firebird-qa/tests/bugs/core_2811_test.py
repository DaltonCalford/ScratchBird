#coding:utf-8

"""
ID:          issue-1179
ISSUE:       1179
TITLE:       'Unsuccessful execution' error when updating a record
DESCRIPTION:
  CREATE OR ALTER TRIGGER NEW_TRIGGER FOR NEW_TABLE
  ACTIVE AFTER UPDATE POSITION 20
  AS
  -- Typically this variable would be used in the trigger. To make it simple, I've
  -- removed all code possible to still get the error.
  declare variable NewHours numeric(9,2) = 0;
  BEGIN
  -- Remove the coalesce and the update will work.
  -- Remove the multiplication and the update will work.
  -- Remove this unneeded variable and the update will work.
  NewHours = coalesce(new.new_field * (0), 0);

  -- Remove this exit command and the update will work. I know it's not needed,
  -- however in the scenario I ran into this there was much more code after the
  -- exit, and the exit was inside an IF clause. This is just the simplest way of reproducing it.
  exit;
  end
  ^

  -- Now update the record to see the error:
  update new_table set new_field = 6;
JIRA:        CORE-2811
FBTEST:      bugs.core_2811
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE NEW_TABLE (
    NEW_FIELD NUMERIC(9,2)
);

INSERT INTO NEW_TABLE (NEW_FIELD) VALUES (4);

SET TERM ^ ;

CREATE OR ALTER TRIGGER NEW_TRIGGER FOR NEW_TABLE
ACTIVE AFTER UPDATE POSITION 20
AS
declare variable NewHours numeric(9,2) = 0;
BEGIN
NewHours = coalesce(new.new_field * (0), 0);
exit;
end
^
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """update new_table set new_field = 6;
select new_field from new_table;
"""

act = isql_act('db', test_script)

expected_stdout = """
   NEW_FIELD
============
        6.00
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

