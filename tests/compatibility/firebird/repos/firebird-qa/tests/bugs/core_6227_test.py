#coding:utf-8

"""
ID:          issue-6471
ISSUE:       6471
TITLE:       isc_info_svc_user_dbpath always returns alias of main security database
DESCRIPTION:
  String returned by sevrice manager for 'info_user_dbpath' query must contain PATH + file/alias
  rather than only file name or alias of security.db.
  If we call os.path.split() then this string will be splitted onto PATH and ALIAS.
  The first token (PATH) must contain at least one character if we try to split it using os.sep delimiter.
  We check that length of this path is more than zero.
  Note that befor fix reply was: ('', security.db') - so the PATH was empty string rather that None!
JIRA:        CORE-6227
FBTEST:      bugs.core_6227
"""

import pytest
import os
from firebird.qa import *

db = db_factory()

act_1 = python_act('db')

@pytest.mark.version('>=3.0.5')
def test_1(act_1: Action):
    with act_1.connect_server() as srv:
        assert os.path.split(srv.info.security_database)[0]
