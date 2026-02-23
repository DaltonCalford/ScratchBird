#coding:utf-8

"""
ID:          issue-3883
ISSUE:       3883
TITLE:       Support for WHEN SQLSTATE
DESCRIPTION:
JIRA:        CORE-3526
FBTEST:      bugs.core_3526
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns(msg varchar(1000)) as
        declare c smallint = 32767;
    begin
        msg='';
        begin
            c = c+1;
        when SQLSTATE '22003' do
            begin
               msg = 'got exception with sqlstate = ' || sqlstate || '; ' ;
            end
        end
        suspend;
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             got exception with sqlstate = 22003;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

