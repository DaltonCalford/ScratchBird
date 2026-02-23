#coding:utf-8

"""
ID:          issue-1578
ISSUE:       1578
TITLE:       FB 3.0 crashes on EXIT (or QUIT) command if use UTF8-collation + create domain based on it + issue SHOW DOMAIN
DESCRIPTION:
JIRA:        CORE-4557
FBTEST:      bugs.core_4557
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
  create collation name_coll for utf8 from unicode CASE INSENSITIVE;
  create collation nums_coll for utf8 from unicode CASE INSENSITIVE 'NUMERIC-SORT=1';
  commit;
  create domain dm_name as varchar(80) character set utf8 collate name_coll;
  create domain dm_nums as varchar(20) character set utf8 collate nums_coll;
  commit;
  show domain; -- FB crashes if this will be uncommented
  exit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_3x = """
    DM_NAME DM_NUMS
"""

expected_stdout_5x = """
    DM_NAME
    DM_NUMS
"""

expected_stdout_6x = """
    PUBLIC.DM_NAME
    PUBLIC.DM_NUMS
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
