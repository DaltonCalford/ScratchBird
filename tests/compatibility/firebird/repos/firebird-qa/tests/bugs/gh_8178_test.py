#coding:utf-8

"""
ID:          issue-8178
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8178
TITLE:       Problem with boolean conversion to string inside DataTypeUtil::makeFromList()
DESCRIPTION:
NOTES:
    [11.07.2024] pzotov
    Confirmed problem on 6.0.0.389.
    Checked on 6.0.0.392, 5.0.1.1434, 4.0.5.3127, 3.0.12.33765
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    -- set echo on;
    select coalesce(1, 'c1') from rdb$database;

    select coalesce('c2', 2) from rdb$database;

    select coalesce('c3', true) from rdb$database;

    select coalesce(true, 'c4') from rdb$database;

    select coalesce(5, true) from rdb$database;

    select coalesce(true, 6) from rdb$database;

    ----------------------------------------------------

    select coalesce('c7', true, 7) from rdb$database;

    select coalesce('c8', 8, true) from rdb$database;

    select coalesce(true, 'c9', 9) from rdb$database;

    select coalesce(true, 10, 'c10') from rdb$database;

    select coalesce(11, 'c11', true) from rdb$database;

    select coalesce(12, true, 'c12') from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    1           
    c2          
    c3       
    TRUE     

    Statement failed, SQLSTATE = HY004
    SQL error code = -104
    -Datatypes are not comparable in expression COALESCE

    Statement failed, SQLSTATE = HY004
    SQL error code = -104
    -Datatypes are not comparable in expression COALESCE

    c7          
    c8          
    TRUE        
    TRUE        
    11          
    12          
"""

@pytest.mark.version('>=3.0.12')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
