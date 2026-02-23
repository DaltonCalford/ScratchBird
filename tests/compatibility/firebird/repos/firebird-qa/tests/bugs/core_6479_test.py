#coding:utf-8

"""
ID:          issue-6710
ISSUE:       6710
TITLE:       COMMENT ON USER can only apply comment on user defined by the default usermanager plugin
DESCRIPTION:
  There is no sense to check for Legacy_UserManarer: comment for user will not be stored
  in the sec$users for this plugin. Test verifies only Srp. Discussed with Alex, 12.03.2021
JIRA:        CORE-6479
FBTEST:      bugs.core_6479
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='tmp$c6479_srp', password='123', plugin='Srp')

test_script = """
    comment on user tmp$c6479_srp using plugin Srp is 'This is description for Srp-user';
    --                            ================
    commit;

    set list on;
    set count on;
    select s.sec$user_name, s.sec$plugin, s.sec$description as sec_descr_blob_id
    from sec$users s
    where s.sec$user_name  = upper('tmp$c6479_srp')
    ;
"""

act = isql_act('db', test_script, substitutions=[('SEC_DESCR_BLOB_ID .*', ''), ('[\t ]+', ' ')])

expected_stdout = """
    SEC$USER_NAME                   TMP$C6479_SRP
    SEC$PLUGIN                      Srp
    This is description for Srp-user
    Records affected: 1
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action, tmp_user):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
