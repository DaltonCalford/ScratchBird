#coding:utf-8

"""
ID:          issue-6532
ISSUE:       6532
TITLE:       Hex number used at end of statement (e.g. CREATE DOMAIN ... DEFAULT) may read
  invalid memory and produce wrong values or exceptions
DESCRIPTION:
JIRA:        CORE-6290
FBTEST:      bugs.core_6290
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- All following statements should not produce neither STDOUT nor STDERR:

    create domain dm_test_01 as double precision default 100000000;
    create domain dm_test_02 as bigint default 0xf0000000;
    ----------------------------------------------------------------
    create domain dm_test_03 as int default 1;
    create domain dm_test_04 as bigint default 0xf0000000;
    ----------------------------------------------------------------
    create domain dm_test_05 as date default 'TODAY';
    create domain dm_test_06 as bigint default 0x0F0000000;
    ----------------------------------------------------------------
    create domain dm_test_07 as boolean default true;
    create domain dm_test_08 as bigint default 0x0F0000000;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.execute()
