#coding:utf-8

"""
ID:          issue-1283
ISSUE:       1283
TITLE:       RIGHT() fails with multibyte text blobs > 1024 chars
DESCRIPTION:
JIRA:        CORE-3228
FBTEST:      bugs.core_3228
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """with q (s) as (
        select
            cast(
                cast('AAA' as char(1021)) || 'ZZZ'
            as blob sub_type text character set utf8
        )
        from rdb$database
    )
    select right(s, 3) from q;
with q (s) as (
        select
            cast(
                cast('AAA' as char(1022)) || 'ZZZ'
            as blob sub_type text character set utf8
        )
        from rdb$database
    )
    select right(s, 3) from q;"""

act = isql_act('db', test_script)

expected_stdout = """
            RIGHT
=================
              0:3
==============================================================================
RIGHT:
ZZZ
==============================================================================

            RIGHT
=================
              0:8
==============================================================================
RIGHT:
ZZZ
==============================================================================
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

