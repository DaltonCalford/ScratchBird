#coding:utf-8

"""
ID:          issue-412
ISSUE:       412
TITLE:       Index bug
DESCRIPTION: Can not fetch the data when Index is in use
JIRA:        CORE-86
FBTEST:      bugs.core_0086
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core86.fbk')

test_script = """Select * from YLK A where PH = '0021'
and HPBH = '492'
and CD = 'MG'
and JLDW = 'JIAN'
and JZDW = 'DUN'
and CK = '8K'
and HW = '1.8'
and SH='1.81';
"""

act = isql_act('db', test_script)

expected_stdout = """ID_YLK PH                           HPBH CD                   JLDW   JZDW   CK               HW                              SH
============ ==================== ============ ==================== ====== ====== ================ ============ =====================
         110 0021                          492 MG                   JIAN   DUN    8K               1.8                        1.81000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

