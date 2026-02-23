#coding:utf-8

"""
ID:          issue-3313
ISSUE:       3313
TITLE:       DROP VIEW drops output parameters of used stored procedures
DESCRIPTION:
JIRA:        CORE-2930
FBTEST:      bugs.core_2930
NOTES:
    [27.06.2025] pzotov
    Removed 'SHOW' commands from test as they can be replaced with query to rdb$procedure_parameters.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    create procedure sp_test returns (o_result integer) as
    begin
        o_result = 1;
        suspend;
    end ^
    set term ;^
    create view v1 as select * from sp_test;
    commit;
    drop view v1;
    select
        pp.rdb$parameter_name
        ,pp.rdb$parameter_number
        ,pp.rdb$parameter_type
    from rdb$procedure_parameters pp where pp.rdb$procedure_name = upper('sp_test');
    select * from sp_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$PARAMETER_NAME              O_RESULT
    RDB$PARAMETER_NUMBER            0
    RDB$PARAMETER_TYPE              1
    O_RESULT                        1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

