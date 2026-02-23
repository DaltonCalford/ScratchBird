#coding:utf-8

"""
ID:          issue-7600
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7600
TITLE:       CASE statement different result compared to FB 2.5.9
DESCRIPTION:
NOTES:
    [02.10.2023] pzotov
    Confirmed bug on 4.0.4.2998, 5.0.0.1235.
    Checked on 6.0.0.65 - all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select replace(
           case when 'N' = 'Y' then cast('YES' as varchar(30)) 
                when 'N' = 'N' then 'NO' 
                else 'DUNNO' 
           end,
           ' ', '_') as test
    from rdb$database;

    select replace(
           case when 'N' = 'Y' then cast('YES' as varchar(30)) 
                when 'N' = 'X' then 'XXX' 
                when 'N' = 'N' then 'NO' 
           end,
           ' ', '_') as test
      from rdb$database;

    select replace(
           case 'N'
              when 'Y' then 'YES' 
              when 'X' then 'XXX' 
              when 'N' then 'NO' 
           end,
           ' ', '_') as test
      from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    NO
    NO
    NO
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
