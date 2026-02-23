#coding:utf-8

"""
ID:          issue-5961
ISSUE:       5961
TITLE:       Position function does not consider the collation for blob
DESCRIPTION:
JIRA:        CORE-5695
FBTEST:      bugs.core_5695
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    set term ^;
    execute block returns (res smallint) as
        declare blb blob sub_type 1 segment size 80 collate unicode_ci;
        declare txt varchar(255) collate unicode_ci;
    begin
        -- pure ASCII strings:
        blb = 'A';
        txt = 'a';
        res = position(txt, blb);
        suspend;
        -- strings with NON-ascii characters:
        blb=  'ŁÁTÉÇØΙΚΌΛΑΟΣ';
        txt = 'Łátéçøικόλαος';
        res = position(txt, blb);
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    RES                             1
    RES                             1
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout
