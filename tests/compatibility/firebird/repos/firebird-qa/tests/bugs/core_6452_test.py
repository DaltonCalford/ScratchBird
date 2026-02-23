#coding:utf-8

"""
ID:          issue-6685
ISSUE:       6685
TITLE:       SIMILAR TO leads to an infinite loop
DESCRIPTION:
  One more test to check 're2' library.
JIRA:        CORE-6452
FBTEST:      bugs.core_6452
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select
      1
    from
      RDB$DATABASE
    where
      '<licenc>
            <items>
                <item title="CallCenter" value="false"/>
                <item title="CRM" value="false"/>
                <item title="Securities" value="false"/>
                <item title="Credits" value="false"/>
                <item title="Registry" value="true"/>
                <item title="Campaign" value="true"/>
                <item title="Damages" value="false"/>
                <item title="Prints" value="false"/>
                <item title="Online" value="true"/>
             </items>
       </licenc>' similar to '%title="_{3,40}" value="true"%';
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
