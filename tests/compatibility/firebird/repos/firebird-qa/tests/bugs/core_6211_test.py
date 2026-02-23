#coding:utf-8

"""
ID:          issue-6456
ISSUE:       6456
TITLE:       Command "ISQL -X" can not extract ROLE name when use multi-byte charset for
  connection (4.x only is affected)
DESCRIPTION:
JIRA:        CORE-6211
FBTEST:      bugs.core_6211
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
	CREATE ROLE NACHALNIK4UKOTKINACHALNIK4UKOTKINACHALNIK4UKOTKINACHALNIK4UKOTK;
	CREATE ROLE "НачальникЧукоткиНачальникЧукоткиНачальникЧукоткиНачальникЧукотк";
	CREATE ROLE "‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰";
	CREATE ROLE "🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀";
"""

ddl_script = """
    set bail on;
    create role  Nachalnik4ukotkiNachalnik4ukotkiNachalnik4ukotkiNachalnik4ukotk;  -- ASCII only
    create role "НачальникЧукоткиНачальникЧукоткиНачальникЧукоткиНачальникЧукотк"; -- Cyrillic, two bytes per character
    create role "‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰‰"; -- U+2030 PER MILLE SIGN, three bytes per character: E2 80 B0
    create role "🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀🚀"; -- U+1F680 ROCKET, four bytes per character: F0 9F 9A 80
    commit;
    set list on;
    set count on;
    select rdb$role_name as r_name from rdb$roles where rdb$system_flag is distinct from 1;
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.isql(switches=[], input=ddl_script)
    act.reset()
    act.expected_stdout = expected_stdout
    act.isql(switches=['-x'])
    act.stdout = '\n'.join([line for line in act.stdout.splitlines() if 'CREATE ROLE' in line])
    assert act.clean_stdout == act.clean_expected_stdout
