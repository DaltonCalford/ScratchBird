#coding:utf-8

"""
ID:          issue-2306
ISSUE:       2306
TITLE:       Error on script with current_date
DESCRIPTION:
  It seems that bug was somehow related to "if" statement. For example, following statements:
    select 1 from new_table nt where nt.data_reg = cast(current_date as date) into c;
  or:
    select 1 from rdb$database
    where not exists(
        select 1 from new_table nt where nt.data_reg = cast(current_date as date)
    )
    into c;
  -- finished without errors.
JIRA:        CORE-1875
FBTEST:      bugs.core_1875
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table new_table (
        data_reg timestamp -- can be used in this test instead of type 'date' (result the same: 2.1.0 crashes)
    );
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set term ^;
    execute block returns(msg varchar(10)) as
        declare c int;
    begin
        if ( exists( select 1 from new_table nt where nt.data_reg = cast(current_date as date) ) ) then
            begin
            end
        msg = 'Done';
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             Done
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

