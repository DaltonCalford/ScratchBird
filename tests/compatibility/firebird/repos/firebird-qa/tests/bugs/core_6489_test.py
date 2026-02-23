#coding:utf-8

"""
ID:          issue-6719
ISSUE:       6719
TITLE:       User without ALTER ANY ROLE privilege can use COMMENT ON ROLE
DESCRIPTION:
JIRA:        CORE-6489
FBTEST:      bugs.core_6489
    [03.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

user_jr = user_factory('db', name='tmp$c6489_junior', password='123', plugin='Srp')
user_sr = user_factory('db', name='tmp$c6489_senior', password='456', plugin='Srp')

test_script = """
    grant alter any role to user tmp$c6489_senior;
    commit;

    connect '$(DSN)' user tmp$c6489_junior password '123';
    comment on role rdb$admin is 'Comment by tmp$c6489_junior';
    commit;

    connect '$(DSN)' user tmp$c6489_senior password '456';
    comment on role rdb$admin is 'Comment by tmp$c6489_senior';
    commit;

    set list on;
    select r.rdb$description as role_descr_blob_id from rdb$roles r where r.rdb$role_name = upper('rdb$admin');
    commit;

    comment on role rdb$admin is null;
    commit;
"""

substitutions = [
    ('[\t ]+', ' '),
    ('ROLE_DESCR_BLOB_ID .*', ''),
    ('(-)?Effective user is.*', ''),
]

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -COMMENT ON RDB$ADMIN failed
    -no permission for ALTER access to ROLE RDB$ADMIN
    Comment by tmp$c6489_senior
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -COMMENT ON "RDB$ADMIN" failed
    -no permission for ALTER access to ROLE "RDB$ADMIN"
    Comment by tmp$c6489_senior
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action, user_sr, user_jr):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
