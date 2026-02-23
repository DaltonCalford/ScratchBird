#coding:utf-8

"""
ID:          issue-5655
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5655
TITLE:       ISQL should be able to process single statement with length up to 10*1024*1024 chars
DESCRIPTION:
  Source sample see in #5655 / CORE-5382
  Test prepares script with two SELECT statements:
  One of them has length EXACTLY equal to 10*1024*1024 characters (excluding final ';'),
  second has length = 10*1024*1024 + 1.

  First statement should be executed OK, second should fail.
  Checked on WI-V3.0.2.32625, WI-T4.0.0.440

  PS. Best place of this test in functional folder rather than in 'issues' one.
JIRA:       CORE-5382
FBTEST:      functional.basic.isql.05
"""

import pytest
from pathlib import Path
from firebird.qa import *

init_script = """
    recreate table dua(i int);
    insert into dua(i) values(1);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('After line .*', ''), ('-At line.*', ''), ('At line.*', '')])

expected_stdout = """
      I                               10485760
"""

expected_stderr = """
    Statement failed, SQLSTATE = 54000
    Dynamic SQL Error
    -SQL error code = -902
    -Implementation limit exceeded
    -SQL statement is too long. Maximum size is 10485760 bytes.
"""

script_file = temp_file('script.sql')

@pytest.mark.version('>=3.0')
def test_1(act: Action, script_file: Path):
    with script_file.open(mode='w') as f:
        f.write('set list on;\n')
        f.write(f"select /*{'-' * 10485720}*/ 10485760 i from rdb$database;\n")
        f.write(f"select /*{'-' * 10485721}*/ 10485761 i from rdb$database;\n")
    act.expected_stderr = expected_stderr
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input_file=script_file)
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
