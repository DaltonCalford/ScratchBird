#coding:utf-8

"""
ID:          issue-3701
ISSUE:       3701
TITLE:       Wrong results (internal wrapping occured) for the multi-byte blob SUBSTRING
  function and its boundary arguments
DESCRIPTION:
JIRA:        CORE-3335
FBTEST:      bugs.core_3335
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select substring(cast('abcdef' as blob sub_type text character set utf8)
from 1 for 2147483647) from rdb$database;
select substring(cast('abcdef' as blob sub_type text character set utf8)
from 2 for 2147483647) from rdb$database;
select substring(cast('abcdef' as blob sub_type text character set utf8)
from 3 for 2147483647) from rdb$database;
select substring(cast('abcdef' as blob sub_type text character set utf8)
from 3 for 2147483646) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
        SUBSTRING
=================
              0:2
==============================================================================
SUBSTRING:
abcdef
==============================================================================

        SUBSTRING
=================
              0:6
==============================================================================
SUBSTRING:
bcdef
==============================================================================

        SUBSTRING
=================
              0:a
==============================================================================
SUBSTRING:
cdef
==============================================================================

        SUBSTRING
=================
              0:e
==============================================================================
SUBSTRING:
cdef
==============================================================================
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

