#coding:utf-8

"""
ID:          issue-7610
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7610
TITLE:       Uninitialized/random value assigned to RDB$ROLES -> RDB$SYSTEM PRIVILEGES when restoring from FB3 backup
DESCRIPTION: 
    Test uses .fbk which was created in FB 3.x as it is described in the ticket.
    Non-privileged user 'tmp_user_7610' and role 'tmp_role_7610' are created, and their names must be exactly the same
    as used in FB 3.x.
    We restore from this DB and check that it completes OK.
    Then we make connection as non-pritileged user using role that did exist in FB 3.x.
    Query 'select rdb$role_name,rdb$system_privileges from rdb$roles' must show 0000000000000000 for this role.
    Query 'select * from test' must fail with 'no permission for SELECT' error.
NOTES:
    [03.06.2023] pzotov
        BOTH problems (ability to query table and random numbers in rdb$system_privileges) could be reproduced only in OLD
        snapshots, not in recent ones! 
        In FB 4.x last snapshot where *both* problems present is 4.0.0.2571 (20-aug-2021). In 4.0.0.2573 only problem with
        random number in rdb$ exists, but user can no longer query table.
        In 4.0.3.2948 (01-jun-2023) content of rdb$ is 0000000000000000.

        In FB 5.x situation is similar: last snapshot with *both* problems is 5.0.0.1000 (02-apr-2023), and since 5.0.0.1001
        one may see only problem with numbers in rdb$, but they look 'constant': 3400000000000000, and this is so up to 5.0.0.1063.
        Since 5.0.0.1065 (01-jun-2023) content of rdb$ is 0000000000000000.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *
import zipfile
from pathlib import Path
from firebird.driver import SrvRestoreFlag
import locale
import re
import time

db = db_factory() # do_not_create = True)
tmp_user = user_factory('db', name='tmp_user_7610', password='123')
tmp_role = role_factory('db', name='tmp_role_7610')

act = python_act('db')

fbk_file = temp_file('gh_7610.tmp.fbk')

@pytest.mark.version('>=4.0.3')
def test_1(act: Action, fbk_file: Path, tmp_user: User, tmp_role: Role, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_7610.zip', at = 'gh_7610_made_in_fb_3x.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())

    allowed_patterns = \
    (
         'gbak:finishing, closing, and going home'
        ,'gbak:adjusting the ONLINE and FORCED WRITES flags'
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.database.restore(database=act.db.db_path, backup=fbk_file, flags=SrvRestoreFlag.REPLACE, verbose=True)
        restore_log = srv.readlines()
        for line in restore_log:
            if act.match_any(line.strip(), allowed_patterns):
                print(line)

    expected_stdout = """
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #####################################################

    test_sql = f"""
        set list on;
        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}' role {tmp_role.name};
        select mon$user, mon$role from mon$attachments where mon$attachment_id = current_connection;
        select rdb$role_name,rdb$system_privileges from rdb$roles;
        select * from test;
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TEST_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout = f"""
        MON$USER                        {tmp_user.name.upper()}
        MON$ROLE                        {tmp_role.name.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        RDB$ROLE_NAME                   {tmp_role.name.upper()}
        RDB$SYSTEM_PRIVILEGES           0000000000000000
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {tmp_user.name.upper()}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, connect_db = False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
