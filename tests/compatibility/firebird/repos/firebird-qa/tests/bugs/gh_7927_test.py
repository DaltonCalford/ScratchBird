#coding:utf-8

"""
ID:          issue-7927
ISSUE:       7927
TITLE:       Some default values is set incorrectly for SC/CS architectures
DESCRIPTION:
NOTES:
    Confirmed bug on 6.0.0.180.
    Checked on intermediate builds:
         6.0.0.186,  commit 305c40a05b1d64c14dbf5f25f36c42c44c6392d9
         5.0.1.1307, commit e2999cd3d767dc4620cad74c1ea36936ba5cc319
         4.0.5.3042, commit f7b090043e8886ab6286f8d626dd1684dc09e3b8
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
    set list on;
    select rdb$config_name, rdb$config_default
    from rdb$config where rdb$config_name in ('TempCacheLimit', 'DefaultDbCachePages', 'GCPolicy')
    order by rdb$config_name
    ;
"""

expected_stdout = """
    RDB$CONFIG_NAME                 DefaultDbCachePages
    RDB$CONFIG_DEFAULT              256

    RDB$CONFIG_NAME                 GCPolicy
    RDB$CONFIG_DEFAULT              cooperative

    RDB$CONFIG_NAME                 TempCacheLimit
    RDB$CONFIG_DEFAULT              8388608
"""

@pytest.mark.version('>=4.0.5')
def test_1(act: Action):
    if act.vars['server-arch'] != 'Classic':
        pytest.skip("No need to run on Super or SuperClassic.")

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
