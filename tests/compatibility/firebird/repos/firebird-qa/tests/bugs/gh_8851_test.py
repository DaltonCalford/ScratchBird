#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8851
TITLE:       'Internal error' when calling outer procedure after deleting unused result variable from inner procedure
DESCRIPTION:
NOTES:
    [14.01.2026] pzotov
    Confirmed bug on 6.0.0.1389-f784b93; 5.0.4.1730-a42ec44; 4.0.7.3243-85d7ab6.
    Checked on 6.0.0.1389-7f71e94; 5.0.4.1746-a42ec44;
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set term ^;
    create or alter procedure sp1
    returns (field1 integer, field2 integer)
    as begin
      field1 = 1;
      field2 = 2;  
      suspend;
    end 
    ^
    create or alter procedure sp2
    returns (field1 integer) 
    as begin
      field1 = 1;
      if (exists(select 1 from sp1 where field1 = 2)) then
        suspend;
    end 
    ^
    set term ;^
    commit;

    select * from sp2;
    commit;

    set term ^;
    create or alter procedure sp1
    returns (field1 integer) 
    as begin
      field1 = 1;
      suspend;
    end 
    ^
    set term ;^
    set list on;
    select * from sp2; 

"""

substitutions = []
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
