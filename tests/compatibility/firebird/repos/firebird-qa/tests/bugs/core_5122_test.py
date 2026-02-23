#coding:utf-8

"""
ID:          issue-5406
ISSUE:       5406
TITLE:       Expression index may not be used by the optimizer if created and used in different connection charsets
DESCRIPTION:
JIRA:        CORE-5122
FBTEST:      bugs.core_5122
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_sql=  """
    set planonly;
    select * from test where 'zxc' || s starting with 'qwe';
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect(charset='iso8859_1') as con1:
        cur1 = con1.cursor()
        cur1.execute("recreate table test(s varchar(10))")
        cur1.execute("create index test_calc_s on test computed by ('zxc' || s)")
        con1.commit()

    expected_stdout_5x = """
        PLAN (TEST INDEX (TEST_CALC_S))
    """
    expected_stdout_6x = """
        PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_CALC_S")) 
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x

    act.isql(switches=['-q', '-n'], input = test_sql)
    assert act.clean_stdout == act.clean_expected_stdout
