#coding:utf-8

"""
ID:          issue-7176
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7176
TITLE:       Incorrect error "Invalid token. Malformed string." with union + blob + non utf8 varchar
NOTES:
    [27.02.2023] pzotov
    Confirmed bug on 5.0.0.488 (date of build: 25-apr-2022).
    Checked on 5.0.0.959, 4.0.3.2903, 3.0.11.33664 -- all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions = [('BLOB_ID\\s+\\d+:\\d+', 'BLOB_ID')])

expected_stdout = """
    BLOB_ID                         <null>
    BLOB_ID                         0:2
    öüóőúéáűí
"""

@pytest.mark.version('>=3.0.10')
def test_1(act: Action):
    test_script = """
        set list on;
        set blob all;
        create table test (txt varchar(10) character set win1250 collate pxw_hundc);
        insert into test(txt) values ('öüóőúéáűí');
        select cast(null as blob sub_type 1) as blob_id
        from rdb$database
        union all
        select txt
        from test;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], charset = 'utf8', input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
