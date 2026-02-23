#coding:utf-8

"""
ID:          issue-5361
ISSUE:       5361
TITLE:       Lost the charset ID in selection of array element
DESCRIPTION:
JIRA:        CORE-5074
FBTEST:      bugs.core_5074
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(
        array_col char(10)[0:3] character set octets
    );
    set sqlda_display on;
    select array_col[0] from test;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    expected_stdout_5x = """
        INPUT message field count: 0
        OUTPUT message field count: 1
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 10 charset: 1 OCTETS
        :  name: ARRAY_COL  alias: ARRAY_COL
        : table: TEST  owner: SYSDBA
    """

    expected_stdout_6x = """
        INPUT message field count: 0
        OUTPUT message field count: 1
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 10 charset: 1 SYSTEM.OCTETS
        :  name: ARRAY_COL  alias: ARRAY_COL
        : table: TEST  schema: PUBLIC  owner: SYSDBA
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output =True)
    assert act.clean_stdout == act.clean_expected_stdout

