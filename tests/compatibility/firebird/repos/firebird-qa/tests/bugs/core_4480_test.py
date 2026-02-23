#coding:utf-8

"""
ID:          issue-4800
ISSUE:       4800
TITLE:       ISQL issues warning: "Bad debug info format" when connect to database with stored function after it`s restoring
DESCRIPTION:
JIRA:        CORE-4480
FBTEST:      bugs.core_4480
"""

import pytest
from firebird.qa import *

init_script = """
    --    Note: core4480.fbk was created by WI-T3.0.0.30809 Firebird 3.0 Alpha 2.
    --    Retoring of this file in WI-T3.0.0.30809 finishes with:
    --    gbak: WARNING:function FN_A is not defined
    --    gbak: WARNING:    module name or entrypoint could not be found
    --    gbak: WARNING:function FN_A is not defined
    --    gbak: WARNING:    module name or entrypoint could not be found
    --    2) Attempt `execute procedure sp_a;` - leads to:
    --    Statement failed, SQLSTATE = 39000
    --    invalid request BLR at offset 29
    --    -function FN_A is not defined
    --    -module name or entrypoint could not be found
    --    -Error while parsing procedure SP_A's BLR
"""

db = db_factory(from_backup='core4480.fbk', init=init_script)

test_script = """
    execute procedure sp_a;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
