#coding:utf-8

"""
ID:          issue-7729
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7729
TITLE:       "SET BIND OF TS WITH TZ TO VARCHAR(128)" uses the date format of dialect 1
DESCRIPTION:
NOTES:
    [25.08.2023] pzotov
    Confirmed problem on 5.0.0.1169
    Checked on 5.0.0.1177 (intermediate snapshot).
    ::: NB :::
    Currently FB 4.x seems still having problem. There is no appropriate commit in v4.0-release.
    Test min_version for now is 5.0.
"""

import pytest
from firebird.qa import *

db = db_factory()

CHK_TIMESTAMP = '2023-08-29 01:02:03.0123 +03:00'
test_script = f"""
    set heading off;
    SET BIND OF TIMESTAMP WITH TIME ZONE TO varchar(128);
    select timestamp '{CHK_TIMESTAMP}' dts from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = f"""
    {CHK_TIMESTAMP}
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
