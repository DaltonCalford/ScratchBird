#coding:utf-8

"""
ID:          tabloid.eqc-344124
TITLE:       Check ability to run selectable SP with input parameter which inserts into GTT
  (on commit DELETE rows) and then does suspend
DESCRIPTION:
  NB: if either a_id, suspend or the insert is removed, or if gtt_test is changed to on commit preserve rows - no crash
FBTEST:      functional.tabloid.eqc_344124
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate global temporary table gtt_test (
        id  integer
    ) on commit delete rows;

    set term  ^;
    create procedure test
    returns (
        o_id integer)
    as
    begin
      insert into gtt_test(id) values( 1 + rand() * 100 ) returning sign(id) into o_id;
      --o_id = 0;
      suspend;
    end
    ^
    set term ;^
    commit;

    set list on;
    select * from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    O_ID                            1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
