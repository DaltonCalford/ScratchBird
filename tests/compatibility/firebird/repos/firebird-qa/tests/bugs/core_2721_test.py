#coding:utf-8

"""
ID:          issue-3117
ISSUE:       3117
TITLE:       Issue with SIMILAR TO and UTF8 on 2.5 Beta 2 (and 1)
DESCRIPTION:
JIRA:        CORE-2721
FBTEST:      bugs.core_2721
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core2721.fbk')

test_script = """
    set list on;
    set count on;
    select * from test where utf8field similar to 'DELL %';
    select * from test where utf8field similar to 'DE %';
"""

act = isql_act('db', test_script)

expected_stdout = """
    ANSIFIELD                       DELL COMPUTERS
    UTF8FIELD                       DELL COMPUTERS
    UNICODECIFIELD                  DELL COMPUTERS

    ANSIFIELD                       DELL BV
    UTF8FIELD                       DELL BV
    UNICODECIFIELD                  DELL BV

    ANSIFIELD                       DELL BV-GLOBAL COMMUNITY
    UTF8FIELD                       DELL BV-GLOBAL COMMUNITY
    UNICODECIFIELD                  DELL BV-GLOBAL COMMUNITY

    Records affected: 3

    ANSIFIELD                       DE HEER P.W. BALFOORT
    UTF8FIELD                       DE HEER P.W. BALFOORT
    UNICODECIFIELD                  DE HEER P.W. BALFOORT

    ANSIFIELD                       DE DRIESTAR
    UTF8FIELD                       DE DRIESTAR
    UNICODECIFIELD                  DE DRIESTAR

    ANSIFIELD                       DE SINGEL
    UTF8FIELD                       DE SINGEL
    UNICODECIFIELD                  DE SINGEL

    ANSIFIELD                       DE BOER PLASTIK B.V.
    UTF8FIELD                       DE BOER PLASTIK B.V.
    UNICODECIFIELD                  DE BOER PLASTIK B.V.

    ANSIFIELD                       DE LOOT PC REPAIR
    UTF8FIELD                       DE LOOT PC REPAIR
    UNICODECIFIELD                  DE LOOT PC REPAIR

    ANSIFIELD                       DE HEER P.W. BALFOORT
    UTF8FIELD                       DE HEER P.W. BALFOORT
    UNICODECIFIELD                  DE HEER P.W. BALFOORT

    Records affected: 6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

