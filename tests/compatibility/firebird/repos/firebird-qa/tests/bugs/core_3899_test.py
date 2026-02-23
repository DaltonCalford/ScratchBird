#coding:utf-8

"""
ID:          issue-4235
ISSUE:       4235
TITLE:       row_number(), rank() and dense_rank() return BIGINT datatype in dialect 1
DESCRIPTION:
JIRA:        CORE-3899
FBTEST:      bugs.core_3899
NOTES:
    [11.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    set sqlda_display on;
    select row_number()over() rno, rank()over() rnk, dense_rank()over() drk
    from rdb$database
    rows 0;
    -- NB: on dialect-3 output is:
    -- sqltype: 580 INT64 ...
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype:|name:)).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
      :  name: ROW_NUMBER  alias: RNO
    02: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
      :  name: RANK  alias: RNK
    03: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
      :  name: DENSE_RANK  alias: DRK
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
