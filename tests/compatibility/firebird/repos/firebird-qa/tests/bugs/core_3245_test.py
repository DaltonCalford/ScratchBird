#coding:utf-8

"""
ID:          issue-3615
ISSUE:       3615
TITLE:       SUBSTRING on long blobs truncates result to 32767 if third argument not present
DESCRIPTION:
JIRA:        CORE-3245
FBTEST:      bugs.core_3245
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with q (s) as (
        select cast(cast('abc' as char(32767)) as blob sub_type text)
                 || cast('def' as char(32767))
                 || cast('ghi' as char(32767))
                 || 'tail'
        from rdb$database
    )
    ,r (sub_for, sub_nofor) as (
        select substring(s from 8000 for 120000),
                    substring(s from 8000)
        from q
    )
    select
      char_length(s) as "char_length(s)"
      ,right(s, 3) as "blob_right(s,3)"
      ,char_length(sub_for) as "char_length(sub_for)"
      ,right(sub_for, 3) as "blob_right(sub_for, 3)"
      ,char_length(sub_nofor) as "char_length(sub_nofor)"
      ,right(sub_nofor, 3) as "blob_right(sub_nofor, 3)"
    from q cross join r
    ;
"""

act = isql_act('db', test_script, substitutions=[('blob_.*', '')])

expected_stdout = """
    char_length(s)                  98305
    ail
    char_length(sub_for)            90306
    ail
    char_length(sub_nofor)          90306
    ail
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

