#coding:utf-8

"""
ID:          issue-6053
ISSUE:       6053
TITLE:       User with DROP DATABASE privilege can't drop database
DESCRIPTION:
JIRA:        CORE-5790
FBTEST:      bugs.core_5790
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_user = user_factory('db', name='tmp$c5790', password='123')
fdb_file = temp_file('tmp_5790.fdb')

@pytest.mark.version('>=3.0.4')
def test_1(act: Action, test_user: User, fdb_file: Path):
    if act.is_version('>=4'):
        expected_obj_type = 21
    elif act.is_version('>=3'):
        expected_obj_type = 20

    expected_stdout = f"""
        RDB$USER                        TMP$C5790
        RDB$GRANTOR                     SYSDBA
        RDB$PRIVILEGE                   O
        RDB$GRANT_OPTION                0
        RDB$RELATION_NAME               SQL$DATABASE
        RDB$FIELD_NAME                  <null>
        RDB$USER_TYPE                   8
        RDB$OBJECT_TYPE                 {expected_obj_type}
        Records affected: 1
        Records affected: 0
    """

    act.expected_stdout = expected_stdout
    test_script = f"""
        create database 'localhost:{fdb_file}';
        alter database set linger to 0;
        commit;
        grant drop database to {test_user.name};
        commit;
        connect 'localhost:{fdb_file}' user {test_user.name} password '{test_user.password}';
        set list on;
        set count on;
        select
             r.rdb$user           --           {test_user.name}
            ,r.rdb$grantor        --           sysdba
            ,r.rdb$privilege      --           o
            ,r.rdb$grant_option   --           0
            ,r.rdb$relation_name  --           sql$database
            ,r.rdb$field_name     --           <null>
            ,r.rdb$user_type      --           8
            ,r.rdb$object_type
        from rdb$user_privileges r
        where r.rdb$user=upper('{test_user.name}');

        -- this should NOT show any attachments: "Records affected: 0" must be shown here.
        select * from mon$attachments where mon$attachment_id != current_connection;
        commit;

        drop database;
        rollback;
    """
    act.isql(switches=['-q'], input = test_script, connect_db = False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    assert not fdb_file.exists()
