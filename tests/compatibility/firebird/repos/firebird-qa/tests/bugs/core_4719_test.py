#coding:utf-8

"""
ID:          issue-5026
ISSUE:       5026
TITLE:       Message "Statement failed, SQLSTATE = 00000 + unknown ISC error 0" appears when
  issuing REVOKE ALL ON ALL FROM <existing_user>
DESCRIPTION:
JIRA:        CORE-4719
FBTEST:      bugs.core_4719
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    drop user tmp$c4719;
    commit;
    create user tmp$c4719 password '123';
    commit;
    revoke all on all from tmp$c4719;
    commit;
    drop user tmp$c4719;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('Statement failed, SQLSTATE.*', ''),
                                                 ('record not found for user:.*', '')])

expected_stderr = """
    Warning: ALL on ALL is not granted to TMP$C4719.
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

