#coding:utf-8

"""
ID:          issue-4463
ISSUE:       4463
TITLE:       Sharp-S character treated incorrectly in UNICODE_CI_AI collation
DESCRIPTION:
JIRA:        CORE-4136
FBTEST:      bugs.core_4136
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set list on;
    select
      case when 'Übergeek' collate unicode_ci_ai like 'ÜB%' collate unicode_ci_ai
        then 'match' else 'MISMATCH' end as test_1,
      case when 'Übergeek' collate unicode_ci_ai like 'üb%' collate unicode_ci_ai
        then 'match' else 'MISMATCH' end as test_2,
      case when 'Fußball' collate unicode_ci_ai like 'fu%' collate unicode_ci_ai
        then 'match' else 'MISMATCH' end as test_3,
      case when 'Fußball' collate unicode_ci_ai like 'fuß%' collate unicode_ci_ai
        then 'match' else 'MISMATCH' end as test_4,
      case when upper ('Fußball') like upper ('fuß%')
        then 'match' else 'MISMATCH' end as test_5
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TEST_1                          match
    TEST_2                          match
    TEST_3                          match
    TEST_4                          match
    TEST_5                          match
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

