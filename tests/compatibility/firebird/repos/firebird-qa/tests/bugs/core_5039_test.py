#coding:utf-8

"""
ID:          issue-5326
ISSUE:       5326
TITLE:       Connecting to service with invalid servicename yields incorrect error message
DESCRIPTION:
NOTES:
[28.01.2019]
  Name of service manager is ignored in FB 4.0, see http://tracker.firebirdsql.org/browse/CORE-5883
  ("service_mgr" to be cleaned out from connection string completely...")
[26.05.2021] changed code for FB 4.x and enabled after discuss with Alex.
JIRA:        CORE-5039
FBTEST:      bugs.core_5039
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

act_1 = python_act('db')

expected_stderr_1 = """
    Cannot attach to services manager
    -service qwe_mnb_zxc_9 is not defined
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stderr = expected_stderr_1
    act_1.svcmgr(switches=['localhost:qwe_mnb_zxc_9', 'user', 'SYSDBA',
                           'password', 'masterkey', 'info_server_version'],
                 connect_mngr=False)
    assert act_1.clean_stderr == act_1.clean_expected_stderr

# version: 4.0

act_2 = python_act('db', substitutions=[('Server version: .* Firebird \\d+\\.\\d+.*',
                                         'Server version: Firebird')])

expected_stdout_2 = """
    Server version: Firebird
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.svcmgr(switches=['localhost:qwe_mnb_zxc_9', 'user', 'SYSDBA',
                           'password', 'masterkey', 'info_server_version'],
                 connect_mngr=False)
    assert act_2.clean_stdout == act_2.clean_expected_stdout
